#pragma once
#include <memory>
#include <glm.hpp>
#include <vk.h>
#include "util.h"
#include "RendererObject.h"

struct VoxelizerData
{
	DW_ALIGNED(16)
		glm::mat4 view;
	DW_ALIGNED(16)
		glm::mat4 projection;
	DW_ALIGNED(16)
		glm::vec4 AABB_min;
	DW_ALIGNED(16)
		glm::vec3 AABB_max;
};

struct VisualizerUBO
{
	DW_ALIGNED(16)
		glm::mat4 model;
	DW_ALIGNED(16)
		glm::mat4 view;
	DW_ALIGNED(16)
		glm::mat4 projection;
};

// Per instance data
struct InstanceData
{
	DW_ALIGNED(16)
		glm::vec4 position;
};

class Voxelizer
{
public:
	dw::vk::PipelineLayout::Ptr   m_pipeline_layout;
	dw::vk::GraphicsPipeline::Ptr m_pipeline;
	dw::vk::PipelineLayout::Ptr   m_reset_compute_pipeline_layout;
	dw::vk::PipelineLayout::Ptr   m_reset_instance_compute_pipeline_layout;
	dw::vk::PipelineLayout::Ptr   m_visualizer_compute_pipeline_layout;
	dw::vk::PipelineLayout::Ptr   m_visualizer_graphics_pipeline_layout;
	dw::vk::ComputePipeline::Ptr  m_reset_compute_pipeline;
	dw::vk::ComputePipeline::Ptr  m_reset_instance_compute_pipeline;
	dw::vk::ComputePipeline::Ptr  m_visualizer_compute_pipeline;
	dw::vk::GraphicsPipeline::Ptr m_visualizer_graphics_pipeline;
	dw::vk::Framebuffer::Ptr      m_framebuffer;
	dw::vk::RenderPass::Ptr       m_render_pass;
	dw::vk::Image::Ptr			  m_image;
	dw::vk::ImageView::Ptr		  m_image_view;

	size_t							 m_ubo_size;
	dw::vk::Buffer::Ptr				 m_ubo_data;
	size_t							 m_instance_buffer_size;
	dw::vk::Buffer::Ptr				 m_instance_buffer;
	size_t							 m_indirect_buffer_size;
	dw::vk::Buffer::Ptr				 m_indirect_buffer;
	size_t							 m_visualizer_ubo_size;
	dw::vk::Buffer::Ptr				 m_visualizer_ubo_data;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_ubo;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_image;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_instance_buffer;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_indirect_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_data;
	dw::vk::DescriptorSet::Ptr       m_ds_image;
	dw::vk::DescriptorSet::Ptr       m_ds_instance_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_indirect_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_visualizer_ubo;
	VoxelizerData					 m_data;
	VisualizerUBO					 m_visualizer_transforms;
	RenderObject					 m_cube;

	Voxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height);
	~Voxelizer();

	glm::vec3 get_center() const;
	glm::mat4 get_view() const;
	glm::mat4 get_proj() const;
	glm::vec3 get_cam_pos() const;
	AABB get_AABB() const;
	uint32_t get_voxels_per_side() const;

	void Voxelizer::begin_render(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend);
	void end_render(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_instance_buffer(dw::vk::CommandBuffer::Ptr cmd_buf);
	void begin_render_visualizer(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend);
	void render_voxels(dw::vk::CommandBuffer::Ptr cmd_buf);
	void transition_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_voxelization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_init_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_voxelization_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf);
	void voxelization_visualization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void visualization_main_buffer_memory_barrier(dw::vk::CommandBuffer::Ptr cmd_buf);

private:
	glm::vec3 m_AABB_min;
	glm::vec3 m_AABB_max;
	glm::vec3 m_center;
	float m_length;
	glm::vec3 m_cam_pos;
	glm::vec3 m_cam_forward;
	glm::mat4 m_view;
	glm::mat4 m_proj;
	uint32_t m_voxels_per_side;
	uint32_t m_viewport_width;
	uint32_t m_viewport_height;

	float get_length(glm::vec3 AABB_min, glm::vec3 AABB_max) const;
	void create_descriptor_sets(dw::vk::Backend::Ptr backend);
	void create_voxelization_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state);
	void create_voxel_reset_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_reset_instance_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_visualizer_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_visualizer_graphics_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height);

public:
	const float m_voxel_width;

};