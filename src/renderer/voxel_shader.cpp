#include "renderer/voxel_shader.hpp"

#include <simple-logger.hpp>

std::unique_ptr<VoxelShader> VoxelShader::create(
    const Device* device,
    uint32_t swapchain_image_count
)
{
    auto out = std::make_unique<VoxelShader>();

    out->device = device;

    // Create shader stage.
    auto stage = ShaderStage::create(device, "assets/shaders/voxel.spv", vk::ShaderStageFlagBits::eCompute);

    if (!stage)
    {
        sl::log_error("Failed to create shader stage for VoxelShader.");

        return nullptr;
    }

    // Create uniform descriptor set layouts.
    vk::DescriptorSetLayoutBinding color_buffer_binding(
        0,
        vk::DescriptorType::eStorageImage,
        1,
        vk::ShaderStageFlagBits::eCompute
    );

    vk::DescriptorSetLayoutCreateInfo uniform_descriptor_set_ci(
        {},
        1, &color_buffer_binding
    );

    vk::Result r;

    std::tie(r, out->uniform_descriptor_set_layout) =
        device->logical_device.createDescriptorSetLayout(uniform_descriptor_set_ci);

    if (r != vk::Result::eSuccess)
    {
        sl::log_error("Failed to create uniform descriptor set layout for VoxelShader.");
        return nullptr;
    }

    // Create uniform descriptor pool.
    vk::DescriptorPoolSize pool_size(
        vk::DescriptorType::eStorageImage,
        swapchain_image_count
    );

    vk::DescriptorPoolCreateInfo pool_ci(
        {},
        swapchain_image_count,
        1,
        &pool_size
    );

    std::tie(r, out->uniform_descriptor_pool) = device->logical_device.createDescriptorPool(pool_ci);

    if (r != vk::Result::eSuccess)
    {
        sl::log_error("Failed to create uniform descriptor pool for VoxelShader.");
        return nullptr;
    }

    // Allocate descriptor sets.
    std::vector<vk::DescriptorSetLayout> set_layouts(swapchain_image_count, out->uniform_descriptor_set_layout);

    vk::DescriptorSetAllocateInfo allocate_info(
        out->uniform_descriptor_pool,
        set_layouts
    );

    std::tie(r, out->uniform_descriptor_sets) = device->logical_device.allocateDescriptorSets(allocate_info);

    if (r != vk::Result::eSuccess)
    {
        sl::log_error("Failed to allocate uniform descriptor sets for VoxelShader.");
        return nullptr;
    }

    // Create pipeline.

    out->pipeline = Pipeline::create_compute(
        device,
        { out->uniform_descriptor_set_layout },
        stage->shader_stage_create_info
    );

    if (!out->pipeline)
    {
        sl::log_error("Failed to create the compute pipeline for VoxelShader.");
        return nullptr;
    }

    return out;
}

VoxelShader::~VoxelShader()
{
    device->logical_device.destroy(uniform_descriptor_pool);

    device->logical_device.destroy(uniform_descriptor_set_layout);
}

void VoxelShader::bind(const CommandBuffer* cb, uint32_t current_image_index)
{
    cb->handle.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline->handle);

    cb->handle.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute,
        pipeline->pipeline_layout,
        0,
        1,
        &uniform_descriptor_sets[current_image_index],
        0,
        nullptr
    );
}

void VoxelShader::update_color_buffer_descriptor_sets(const Swapchain* swapchain)
{
    std::vector<vk::DescriptorImageInfo> image_infos(swapchain->images.size());

    std::vector<vk::WriteDescriptorSet> write_ops(swapchain->images.size());

    for (uint32_t i = 0; i < swapchain->images.size(); i++)
    {
        image_infos[i].imageLayout = vk::ImageLayout::eGeneral;
        image_infos[i].imageView = swapchain->image_views[i];
        image_infos[i].sampler = nullptr;

        write_ops[i].dstSet = uniform_descriptor_sets[i];
        write_ops[i].dstBinding = 0;
        write_ops[i].descriptorCount = 1;
        write_ops[i].descriptorType = vk::DescriptorType::eStorageImage;
        write_ops[i].pImageInfo = &image_infos[i];
    }

    device->logical_device.updateDescriptorSets(write_ops, nullptr);
}
