#pragma once

#include "client/renderer/swapchain.hpp"
#include "client/renderer/pipeline.hpp"
#include "client/renderer/shader_stage.hpp"

struct VoxelShader
{
    std::unique_ptr<Pipeline> pipeline;

    vk::DescriptorPool uniform_descriptor_pool;
    vk::DescriptorSetLayout uniform_descriptor_set_layout;

    std::vector<vk::DescriptorSet> uniform_descriptor_sets;

    const Device* device;

	VoxelShader() = default;

	VoxelShader(const VoxelShader&) = delete; // Prevent copies.

	~VoxelShader();

	VoxelShader& operator = (const VoxelShader&) = delete; // Prevent copies.

    static std::unique_ptr<VoxelShader> create(
        const Device* device,
        uint32_t swapchain_image_count
    );

    void update_color_buffer_descriptor_sets(const Swapchain* swapchain);

    void bind(const CommandBuffer* cb, uint32_t current_image_index);
};
