#include "client/renderer/renderer.hpp"

#include <cmath>

#include <simple-logger.hpp>

#include "client/platform/platform.hpp"
#include "client/renderer/device.hpp"
#include "client/renderer/fence.hpp"
#include "client/renderer/renderer_platform.hpp"
#include "client/renderer/swapchain.hpp"
#include "client/renderer/voxel_shader.hpp"

#ifdef NDEBUG
	static constexpr bool enable_validation_layers = false;
#else
	static constexpr bool enable_validation_layers = true;
#endif

#define REQUESTED_SWAPCHAIN_IMAGE_COUNT 3

// Hardcoded validation layers
static std::vector<const char*> validation_layers = {
	"VK_LAYER_KHRONOS_validation"
};

// Hardcoded device extensions
static std::vector<const char*> device_extensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Static helper functions.
static bool check_validation_layer_support();
static bool recreate_swapchain();

static void transition_swapchain_image_to_trace(CommandBuffer* cb, uint32_t image_idx);
static void transition_swapchain_image_to_present(CommandBuffer* cb, uint32_t image_idx);

// Render state.
static struct
{
    vk::Instance vulkan_instance;

    vk::SurfaceKHR surface;

    Device* device;

	Swapchain* swapchain;

	VoxelShader* voxel_shader;

	std::vector<std::unique_ptr<CommandBuffer>> graphics_command_buffers;

	// Sync objects.
	std::vector<vk::Semaphore> image_available_semaphores;

	std::vector<vk::Semaphore> queue_complete_semaphores;

	std::vector<std::unique_ptr<Fence>> in_flight_fences;

	std::vector<Fence*> images_in_flight;

	uint32_t current_image_index;
} renderer_state;

bool renderer_initialize()
{
    sl::log_info("Initializing renderer.");

    if (enable_validation_layers && !check_validation_layer_support())
	{
		sl::log_fatal("One or more requested validation layers do not exist.");
		return false;
	}

	// Vulkan Instance 
	vk::ApplicationInfo app_info(
		"Industria",
		VK_MAKE_VERSION(0, 1, 0),
		"custom_engine",
		VK_MAKE_VERSION(0, 1, 0),
		VK_API_VERSION_1_3
	);

    // Instance Extensions
	auto platform_extensions = platform_get_required_instance_extensions();

	// Instance validation layers.
	auto enabled_validation_layers = validation_layers;

	if (!enable_validation_layers)
	{
		enabled_validation_layers.clear();
	}

	// Create vulkan instance
	vk::InstanceCreateInfo create_info(
		{},
		&app_info,
		enabled_validation_layers,
		platform_extensions
	);

    vk::Result r;

	std::tie(r, renderer_state.vulkan_instance) = vk::createInstance(create_info);

	if (r != vk::Result::eSuccess)
	{
		sl::log_fatal("Failed to create Vulkan instance.");
		return false;
	}

    // Create vulkan surface
	std::optional<vk::SurfaceKHR> _surface = renderer_platform_create_vulkan_surface(renderer_state.vulkan_instance);

	if (!_surface)
	{
		sl::log_fatal("Failed to create vulkan surface.");
		return false;
	}

	renderer_state.surface = *_surface;

    // Create the device
	std::vector<std::string> device_extensions_str(device_extensions.size());
	for (size_t i = 0; i < device_extensions.size(); i++)
	{
		device_extensions_str[i] = device_extensions[i];
	}

	std::vector<std::string> validation_layers_str(validation_layers.size());
	for (size_t i = 0; i < validation_layers.size(); i++)
	{
		validation_layers_str[i] = validation_layers[i];
	}

	renderer_state.device = Device::create(
		renderer_state.vulkan_instance,
		device_extensions_str,
		validation_layers_str,
		renderer_state.surface
	).release();

	if (renderer_state.device == nullptr)
	{
		sl::log_fatal("Failed to create a logical device.");
		return false;
	}
	
	// Get swapchain info
	SwapchainInfo swapchain_info =
		Swapchain::query_info(renderer_state.device, renderer_state.surface, REQUESTED_SWAPCHAIN_IMAGE_COUNT);
	
	// Create the swapchain
	renderer_state.swapchain = Swapchain::create(
		renderer_state.device,
		renderer_state.surface,
		swapchain_info
	).release();

	if (!renderer_state.swapchain)
	{
		sl::log_fatal("Failed to create the swapchain.");
		return false;
	}

	// Create voxel shader.
	renderer_state.voxel_shader = VoxelShader::create(
		renderer_state.device,
		renderer_state.swapchain->images.size()
	).release();

	if (!renderer_state.voxel_shader)
	{
		sl::log_fatal("Failed to create the VoxelShader.");
		return false;
	}

	// Update descriptors.
	renderer_state.voxel_shader->update_color_buffer_descriptor_sets(renderer_state.swapchain);

	// Create command buffers.
	renderer_state.graphics_command_buffers.reserve(renderer_state.swapchain->max_frames_in_flight);

	for (uint32_t i = 0; i < renderer_state.swapchain->max_frames_in_flight; i++)
	{
		auto cb = CommandBuffer::create(renderer_state.device, renderer_state.device->graphics_command_pool, true);

		if (!cb)
		{
			sl::log_fatal("Failed to create command buffers");
			return false;
		}

		renderer_state.graphics_command_buffers.push_back(std::move(cb));
	}

	// Create sync objects
	renderer_state.image_available_semaphores.resize(renderer_state.swapchain->max_frames_in_flight);

	renderer_state.queue_complete_semaphores.resize(renderer_state.swapchain->max_frames_in_flight);

	renderer_state.in_flight_fences.reserve(renderer_state.swapchain->max_frames_in_flight);

	for (uint32_t i = 0; i < renderer_state.swapchain->max_frames_in_flight; i++)
	{
		vk::SemaphoreCreateInfo semaphore_ci;

		std::tie(r, renderer_state.image_available_semaphores[i]) =
			renderer_state.device->logical_device.createSemaphore(semaphore_ci);

		if (r != vk::Result::eSuccess)
		{
			sl::log_error("Failed to create image available semaphore.");
			return false;
		}

		std::tie(r, renderer_state.queue_complete_semaphores[i]) =
			renderer_state.device->logical_device.createSemaphore(semaphore_ci);

		if (r != vk::Result::eSuccess)
		{
			sl::log_error("Failed to create queue complete semaphore.");
			return false;
		}

		auto fence = Fence::create(renderer_state.device, true);

		if (!fence)
		{
			sl::log_error("Failed to create fence.");
			return false;
		}

		renderer_state.in_flight_fences.push_back(std::move(fence)); 
	}

	// Preallocate the in flight images and set them to nullptr;
	renderer_state.images_in_flight.resize(renderer_state.swapchain->images.size(), nullptr);

    return true;
}

void renderer_shutdown()
{
    vk::Result r = renderer_state.device->logical_device.waitIdle();

	for (size_t i = 0; i < renderer_state.image_available_semaphores.size(); i++)
	{
		renderer_state.device->logical_device.destroy(renderer_state.image_available_semaphores[i]);
		renderer_state.device->logical_device.destroy(renderer_state.queue_complete_semaphores[i]);
	}

	renderer_state.in_flight_fences.clear();

	renderer_state.graphics_command_buffers.clear();

	delete renderer_state.voxel_shader;

	delete renderer_state.swapchain;

    delete renderer_state.device;

    renderer_state.vulkan_instance.destroy(renderer_state.surface);

    renderer_state.vulkan_instance.destroy();

    sl::log_info("Successfully shut down renderer.");
}

bool renderer_begin_frame()
{
	if (renderer_state.swapchain->swapchain_out_of_date)
	{
		recreate_swapchain();
		return renderer_begin_frame();
	}

	uint8_t current_frame = renderer_state.swapchain->current_frame;

	// Wait for the current frame
	if (!renderer_state.in_flight_fences[current_frame]->wait())
	{
		sl::log_warn("Failed to wait on an in-flight fence.");
		return false;
	}

	// Acquire next image in swapchain
	auto next_image_index = renderer_state.swapchain->acquire_next_image_index(
		UINT64_MAX, 
		renderer_state.image_available_semaphores[current_frame],
		nullptr
	);

	if (!next_image_index)
	{
		sl::log_warn("Failed to acquire next swapchain image");
		return false;
	}

	renderer_state.current_image_index = *next_image_index;

	// Begin command buffer
	CommandBuffer* command_buffer = renderer_state.graphics_command_buffers[current_frame].get();
	command_buffer->reset();
	command_buffer->begin(false, false, false);

	// Dynamic state
	vk::Viewport viewport;
	viewport.x = 0.0f;
	viewport.y = (float) renderer_state.swapchain->swapchain_info.swapchain_extent.height;
	viewport.width = (float) renderer_state.swapchain->swapchain_info.swapchain_extent.width;
	viewport.height = -(float) renderer_state.swapchain->swapchain_info.swapchain_extent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// Scissor
	vk::Rect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = renderer_state.swapchain->swapchain_info.swapchain_extent.width;
	scissor.extent.height = renderer_state.swapchain->swapchain_info.swapchain_extent.height;

	command_buffer->handle.setViewport(0, 1, &viewport);
	command_buffer->handle.setScissor(0, 1, &scissor);

	transition_swapchain_image_to_trace(command_buffer, renderer_state.current_image_index);

	renderer_state.voxel_shader->bind(command_buffer, renderer_state.current_image_index);

	command_buffer->handle.dispatch(
		static_cast<uint32_t>(std::ceil(renderer_state.swapchain->swapchain_info.swapchain_extent.width / 8.0f)),
		static_cast<uint32_t>(std::ceil(renderer_state.swapchain->swapchain_info.swapchain_extent.height / 8.0f)),
		1
	);

	return true;
}

bool renderer_end_frame()
{
	uint8_t current_frame = renderer_state.swapchain->current_frame;
	CommandBuffer* command_buffer = renderer_state.graphics_command_buffers[current_frame].get();

	transition_swapchain_image_to_present(command_buffer, renderer_state.current_image_index);

	command_buffer->end();

	// Wait if a previous frame is still using this image.
	if (renderer_state.images_in_flight[current_frame] != nullptr)
	{
		renderer_state.images_in_flight[current_frame]->wait();
	}

	// Mark fence as being in use by this image.
	renderer_state.images_in_flight[current_frame] = renderer_state.in_flight_fences[current_frame].get();

	// Reset the fence
	renderer_state.images_in_flight[current_frame]->reset();

	// Submit queue
	vk::PipelineStageFlags stage_flags[1] = {
		vk::PipelineStageFlagBits::eColorAttachmentOutput
	};

	vk::SubmitInfo submit_info(
		1,
		&renderer_state.image_available_semaphores[current_frame],
		stage_flags,
		1,
		&command_buffer->handle,
		1,
		&renderer_state.queue_complete_semaphores[current_frame]
	);

	vk::Result r = renderer_state.device->graphics_queue
		.submit(1, &submit_info, renderer_state.in_flight_fences[current_frame]->handle);

	if (r != vk::Result::eSuccess)
	{
		sl::log_error("Failed to submit queue.");
		return false;
	}

	command_buffer->set_state(CommandBufferState::SUBMITTED);

	bool present_successful = renderer_state.swapchain->present(
		renderer_state.queue_complete_semaphores[renderer_state.swapchain->current_frame],
		renderer_state.current_image_index
	);

	// Give images back to the swapchain
	if (!present_successful && !renderer_state.swapchain->swapchain_out_of_date)
	{
		// Swapchain is not out of date, so the return value indicates a failure.
		sl::log_fatal("Failed to present swap chain image.");
		return false;
	}

	return true;
}

vector2ui renderer_get_framebuffer_size()
{
	return vector2ui {
		renderer_state.swapchain->swapchain_info.swapchain_extent.width,
		renderer_state.swapchain->swapchain_info.swapchain_extent.height
	};
}

static bool check_validation_layer_support()
{
	// Get available validation layers.
	auto [r, available_layers] = vk::enumerateInstanceLayerProperties();

	if (r != vk::Result::eSuccess)
	{
		sl::log_error("Failed to enumerate instane layer properties.");
		return false;
	}

	for (uint32_t i = 0; i < validation_layers.size(); i++)
	{
		bool layer_found = false;

		// Loop through available layers and check if our layers are among them
		for (uint32_t j = 0; j < available_layers.size(); j++)
		{
			if (strcmp(validation_layers[i], available_layers[j].layerName) == 0)
			{
				layer_found = true;
				break;
			}
		}

		if (!layer_found)
		{
            sl::log_error("Failed to find validation layer: `{}`.", validation_layers[i]);
			return false;
		}
	}

	return true;
}

static bool recreate_swapchain()
{
	sl::log_debug("Recreating swapchain.");

	// Wait for the device to idle.
	vk::Result r = renderer_state.device->logical_device.waitIdle();

	if (r != vk::Result::eSuccess)
	{
		sl::log_error("Failed to wait for device to idle.");
		return false;
	}

	// Reset inflight fences,
	for (uint64_t i = 0; i < renderer_state.images_in_flight.size(); i++)
	{
		renderer_state.images_in_flight[i] = nullptr;
	}

	SwapchainInfo new_info =
		Swapchain::query_info(renderer_state.device, renderer_state.surface, REQUESTED_SWAPCHAIN_IMAGE_COUNT);

	delete renderer_state.swapchain;

	renderer_state.swapchain = Swapchain::create(
		renderer_state.device,
		renderer_state.surface,
		new_info
	).release();

	if (!renderer_state.swapchain)
	{
		sl::log_fatal("Failed to recreate the swapchain.");
		return false;
	}

	// Update shaders.
	renderer_state.voxel_shader->update_color_buffer_descriptor_sets(renderer_state.swapchain);

	return true;
}

static void transition_swapchain_image_to_trace(CommandBuffer* cb, uint32_t image_idx)
{
	vk::ImageSubresourceRange access;
	access.aspectMask = vk::ImageAspectFlagBits::eColor;
	access.baseMipLevel = 0;
	access.levelCount = 1;
	access.baseArrayLayer = 0;
	access.layerCount = 1;

	vk::ImageMemoryBarrier barrier(
		vk::AccessFlagBits::eNoneKHR,
		vk::AccessFlagBits::eMemoryWrite,
		vk::ImageLayout::eUndefined,
		vk::ImageLayout::eGeneral,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		renderer_state.swapchain->images[image_idx],
		access
	);

	cb->handle.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe,
		vk::PipelineStageFlagBits::eComputeShader,
		{},
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}

static void transition_swapchain_image_to_present(CommandBuffer* cb, uint32_t image_idx)
{
	vk::ImageSubresourceRange access;
	access.aspectMask = vk::ImageAspectFlagBits::eColor;
	access.baseMipLevel = 0;
	access.levelCount = 1;
	access.baseArrayLayer = 0;
	access.layerCount = 1;

	vk::ImageMemoryBarrier barrier(
		vk::AccessFlagBits::eMemoryWrite,
		vk::AccessFlagBits::eNoneKHR,
		vk::ImageLayout::eGeneral,
		vk::ImageLayout::ePresentSrcKHR,
		VK_QUEUE_FAMILY_IGNORED,
		VK_QUEUE_FAMILY_IGNORED,
		renderer_state.swapchain->images[image_idx],
		access
	);

	cb->handle.pipelineBarrier(
		vk::PipelineStageFlagBits::eComputeShader,
		vk::PipelineStageFlagBits::eBottomOfPipe,
		{},
		0, nullptr,
		0, nullptr,
		1, &barrier
	);
}
