#include "Voxelizer.h"

class GeometryVoxelizer : public Voxelizer
{
public:
	dw::vk::PipelineLayout::Ptr   m_pipeline_layout;
	dw::vk::GraphicsPipeline::Ptr m_pipeline_correct_texcoords;

	GeometryVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height);
	~GeometryVoxelizer();

	glm::vec3 get_center() const;
	glm::mat4 get_view() const;
	glm::mat4 get_proj() const;
	glm::vec3 get_cam_pos() const;
	uint32_t get_voxels_per_side() const;

	void begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend) override;
	void end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf) override;

private:
	glm::vec3 m_cam_pos;
	glm::vec3 m_cam_forward;
	glm::mat4 m_view;
	glm::mat4 m_proj;

	void create_descriptor_sets(dw::vk::Backend::Ptr backend);
	void create_voxelization_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state);
};