#pragma once

#include "Voxelizer.h"

class ComputeVoxelizer : public Voxelizer
{
public:

	ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height);
	void create_voxelizer_pipeline_state(dw::vk::Backend::Ptr backend);

	void begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend) override;
	void voxelize(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects);
	void end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf) override;

private:

	dw::vk::PipelineLayout::Ptr m_pipeline_layout;
	dw::vk::ComputePipeline::Ptr m_pipeline;

	void create_descriptor_sets(dw::vk::Backend::Ptr backend);
};