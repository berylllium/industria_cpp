#pragma once

#include <optional>

#include <vulkan/vulkan.hpp>

std::optional<vk::SurfaceKHR> renderer_platform_create_vulkan_surface(vk::Instance instance);
