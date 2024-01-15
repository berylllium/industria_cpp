#pragma once

#include "client/renderer/command_buffer.hpp"
#include "client/renderer/device.hpp"
#include "client/renderer/render_pass.hpp"

struct Pipeline
{
	vk::Pipeline handle;
	vk::PipelineLayout pipeline_layout;

	const Device* device;

	Pipeline() = default;

	Pipeline(const Pipeline&) = delete; // Prevent copies.

	~Pipeline();

	Pipeline& operator = (const Pipeline&) = delete; // Prevent copies.
	
    static std::unique_ptr<Pipeline> create_compute(
        const Device* device,
        const std::vector<vk::DescriptorSetLayout>& descriptor_set_layouts,
        const vk::PipelineShaderStageCreateInfo& compute_stage_create_info
    );

	static std::unique_ptr<Pipeline> create_graphics(
		const Device* device,
		const RenderPass* render_pass,
		uint32_t vertex_input_stride,
		const std::vector<vk::VertexInputAttributeDescription>& attributes,
		const std::vector<vk::DescriptorSetLayout>& descriptor_set_layouts,
		const std::vector<vk::PipelineShaderStageCreateInfo>& shader_stages,
		const std::vector<vk::PushConstantRange>& push_constant_ranges,
		vk::Viewport viewport,
		vk::Rect2D scissor,
		bool is_wireframe,
		bool depth_test_enabled
	);

	void bind(const CommandBuffer* command_buffer, vk::PipelineBindPoint bind_point);
};
