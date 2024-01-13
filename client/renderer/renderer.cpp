#include "client/renderer/renderer.hpp"

#include <simple-logger.hpp>

#include "client/platform/platform.hpp"
#include "client/renderer/device.hpp"
#include "client/renderer/fence.hpp"
#include "client/renderer/renderer_platform.hpp"
#include "client/renderer/swapchain.hpp"

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

// Render state.
static struct
{
    vk::Instance vulkan_instance;

    vk::SurfaceKHR surface;

    Device* device;

	// Render Passes.
	RenderPass* world_render_pass;
	RenderPass* ui_render_pass;

	Swapchain* swapchain;

	std::vector<vk::Framebuffer> world_framebuffers;

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
	
	// Create the render passs
	renderer_state.world_render_pass = RenderPass::create(
		renderer_state.device,
		swapchain_info.image_format.format,
		swapchain_info.depth_format,
		vector2ui { 0, 0 },
		vector2ui { swapchain_info.swapchain_extent.width, swapchain_info.swapchain_extent.height },
		vector4f { 0.4f, 0.5f, 0.6f, 0.0f },
		1.0f,
		0,
		RenderPassClearFlagBits::COLOR_BUFFER_FLAG | RenderPassClearFlagBits::DEPTH_BUFFER_FLAG |
			RenderPassClearFlagBits::STENCIL_BUFFER_FLAG,
		false,
		true
	).release();

	renderer_state.ui_render_pass = RenderPass::create(
		renderer_state.device,
		swapchain_info.image_format.format,
		swapchain_info.depth_format,
		vector2ui { 0, 0 },
		vector2ui { swapchain_info.swapchain_extent.width, swapchain_info.swapchain_extent.height },
		vector4f { 0.0f, 0.0f, 0.0f, 0.0f },
		1.0f,
		0,
		RenderPassClearFlagBits::NONE_FLAG,
		true,
		false
	).release();
	
	if (!renderer_state.world_render_pass)
	{
		sl::log_fatal("Failed to create the world render pass.");
		return false;
	}

	if (!renderer_state.ui_render_pass)
	{
		sl::log_fatal("Failed to create the ui render pass.");
		return false;
	}

	// Create the swapchain
	renderer_state.swapchain = Swapchain::create(
		renderer_state.device,
		renderer_state.ui_render_pass,
		renderer_state.surface,
		swapchain_info
	).release();

	if (!renderer_state.swapchain)
	{
		sl::log_fatal("Failed to create the swapchain.");
		return false;
	}

	// Create world framebuffers.
	renderer_state.world_framebuffers.resize(renderer_state.swapchain->images.size());

	for (size_t i = 0; i < renderer_state.swapchain->images.size(); i++)
	{
		std::vector<vk::ImageView> attachments = {
			renderer_state.swapchain->image_views[i],
			renderer_state.swapchain->depth_attachments[i]->image_view
		};

		vk::FramebufferCreateInfo fb_ci(
			{},
			renderer_state.world_render_pass->handle,
			attachments,
			renderer_get_framebuffer_size().w,
			renderer_get_framebuffer_size().h,
			1
		);

		std::tie(r, renderer_state.world_framebuffers[i]) =
			renderer_state.device->logical_device.createFramebuffer(fb_ci);
	}

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

	for (size_t i = 0; i < renderer_state.world_framebuffers.size(); i++)
	{
		renderer_state.device->logical_device.destroy(renderer_state.world_framebuffers[i]);
	}

	delete renderer_state.ui_render_pass;

	delete renderer_state.world_render_pass;

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

	renderer_state.world_render_pass
		->begin(command_buffer, renderer_state.world_framebuffers[renderer_state.current_image_index]);

	return true;
}

bool vulkan_end_frame(float delta_time)
{
	uint8_t current_frame = renderer_state.swapchain->current_frame;
	CommandBuffer* command_buffer = renderer_state.graphics_command_buffers[current_frame].get();

	// End render pass.
	renderer_state.world_render_pass->end(command_buffer);

	renderer_state.ui_render_pass
		->begin(command_buffer, renderer_state.swapchain->framebuffers[renderer_state.current_image_index]);

	renderer_state.ui_render_pass->end(command_buffer);

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

	for (size_t i = 0; i < renderer_state.world_framebuffers.size(); i++)
	{
		renderer_state.device->logical_device.destroy(renderer_state.world_framebuffers[i]);
	}
	renderer_state.world_framebuffers.clear();

	delete renderer_state.ui_render_pass;
	delete renderer_state.world_render_pass;
	delete renderer_state.swapchain;

	renderer_state.world_render_pass = RenderPass::create(
		renderer_state.device,
		new_info.image_format.format,
		new_info.depth_format,
		vector2ui { 0, 0 },
		vector2ui { new_info.swapchain_extent.width, new_info.swapchain_extent.height },
		vector4f { 0.4f, 0.5f, 0.6f, 0.0f },
		1.0f,
		0,
		RenderPassClearFlagBits::COLOR_BUFFER_FLAG | RenderPassClearFlagBits::DEPTH_BUFFER_FLAG |
			RenderPassClearFlagBits::STENCIL_BUFFER_FLAG,
		false,
		true
	).release();

	renderer_state.ui_render_pass = RenderPass::create(
		renderer_state.device,
		new_info.image_format.format,
		new_info.depth_format,
		vector2ui { 0, 0 },
		vector2ui { new_info.swapchain_extent.width, new_info.swapchain_extent.height },
		vector4f { 0.0f, 0.0f, 0.0f, 0.0f },
		1.0f,
		0,
		RenderPassClearFlagBits::NONE_FLAG,
		true,
		false
	).release();

	if (!renderer_state.world_render_pass)
	{
		sl::log_fatal("Failed to recreate the world render pass.");
		return false;
	}

	if (!renderer_state.ui_render_pass)
	{
		sl::log_fatal("Failed to recreate the ui render pass.");
		return false;
	}

	renderer_state.swapchain = Swapchain::create(
		renderer_state.device,
		renderer_state.ui_render_pass,
		renderer_state.surface,
		new_info
	).release();

	if (!renderer_state.swapchain)
	{
		sl::log_fatal("Failed to recreate the swapchain.");
		return false;
	}

	// World framebuffers.
	size_t swapchain_image_count = renderer_state.swapchain->images.size();

	renderer_state.world_framebuffers.resize(swapchain_image_count);

	for (size_t i = 0; i < swapchain_image_count; i++)
	{
		std::vector<vk::ImageView> attachments = {
			renderer_state.swapchain->image_views[i],
			renderer_state.swapchain->depth_attachments[i]->image_view
		};

		vk::FramebufferCreateInfo fb_ci(
			{},
			renderer_state.world_render_pass->handle,
			attachments,
			renderer_get_framebuffer_size().w,
			renderer_get_framebuffer_size().h,
			1
		);

		std::tie(r, renderer_state.world_framebuffers[i]) =
			renderer_state.device->logical_device.createFramebuffer(fb_ci);

		if (r != vk::Result::eSuccess)
		{
			sl::log_error("Failed to recreate world frame buffers.");
			return false;
		}
	}

	return true;
}
