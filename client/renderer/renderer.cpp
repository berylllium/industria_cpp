#include "client/renderer/renderer.hpp"

#include <simple-logger.hpp>

#include "client/platform/platform.hpp"
#include "client/renderer/device.hpp"
#include "client/renderer/renderer_platform.hpp"

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

    return true;
}

void renderer_shutdown()
{
    vk::Result r = renderer_state.device->logical_device.waitIdle();

    delete renderer_state.device;

    renderer_state.vulkan_instance.destroy(renderer_state.surface);

    renderer_state.vulkan_instance.destroy();

    sl::log_info("Successfully shut down renderer.");
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
