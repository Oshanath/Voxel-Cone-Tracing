#pragma once

#include "Voxelizer.h"

enum ComputeVoxelizationType
{
	CORRECT_TEXCOORDS,
	INCORRECT_TEXCOORDS
};

class ComputeVoxelizer : public Voxelizer
{
public:

	ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height, std::vector<RenderObject>& objects);
	void create_voxelizer_pipeline_state(dw::vk::Backend::Ptr backend);

	void begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend) override;
	void voxelize(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects);
	void end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf) override;

	inline void set_compute_voxelization_type(ComputeVoxelizationType type) { m_compute_voxelization_type = type; }

private:

	dw::vk::PipelineLayout::Ptr m_pipeline_layout;
	dw::vk::ComputePipeline::Ptr m_pipeline_correct_texcoords;
	dw::vk::ComputePipeline::Ptr m_pipeline_incorrect_texcoords;
	ComputeVoxelizationType m_compute_voxelization_type;

	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_bindless;
	dw::vk::DescriptorSet::Ptr	     m_ds_bindless;
	uint32_t bindless_ds_size;
	dw::vk::Buffer::Ptr m_bindless_buffer;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_bindless_buffer;
	dw::vk::DescriptorSet::Ptr	     m_ds_bindless_buffer;

	void create_descriptor_sets(dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects);
};