#pragma once

#include "Voxelizer.h"

class ComputeVoxelizer : public Voxelizer
{
public:

	ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, VoxelizationType voxelization_type, uint32_t m_viewport_width, uint32_t m_viewport_height);

private:
	void create_descriptor_sets(dw::vk::Backend::Ptr backend);
	void create_pipelines(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state);
};