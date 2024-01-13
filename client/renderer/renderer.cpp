#include "client/renderer/renderer.hpp"

#include <simple-logger.hpp>

#include "client/platform/platform.hpp"
#include "client/renderer/device.hpp"
#include "client/renderer/renderer_platform.hpp"
#include "client/renderer/swapchain.hpp"

#ifdef NDEBUG
	static constexpr bool enable_validation_layers = false;
#else
	static constexpr bool enable_validation_layers = true;
#endif

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
	SwapchainInfo swapchain_info = Swapchain::query_info(renderer_state.device, renderer_state.surface);

	swapchain_info.requested_image_count = 3;

	if (swapchain_info.max_image_count > 0 && swapchain_info.requested_image_count > swapchain_info.max_image_count)
	{
		sl::log_fatal("Requested swapchain image count ({}) is greater than the maximum swapchain image count supported"
			" by the graphics card ({}).", swapchain_info.requested_image_count, swapchain_info.max_image_count);

		return false;
	}

	if (swapchain_info.requested_image_count < swapchain_info.min_image_count)
	{
		sl::log_fatal("Requested swapchain image count ({}) is less than the minimum swapchain image count supported by"
			" the graphics card ({}).", swapchain_info.requested_image_count, swapchain_info.min_image_count);
	}

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

    return true;
}

void renderer_shutdown()
{
    vk::Result r = renderer_state.device->logical_device.waitIdle();

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
