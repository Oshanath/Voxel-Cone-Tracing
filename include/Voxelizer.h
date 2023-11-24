#pragma once
#include <memory>
#include <glm.hpp>
#include <vk.h>
#include "util.h"
#include "RendererObject.h"
#include <gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>
#include <algorithm>
#include <material.h>

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

struct VoxelizerData
{
	DW_ALIGNED(16)
		glm::mat4 view;
	DW_ALIGNED(16)
		glm::mat4 projection;
	DW_ALIGNED(16)
		glm::vec4 AABB_min;
	DW_ALIGNED(16)
		glm::vec4 AABB_max;
};

enum VoxelizationType
{
	GEOMETRY_SHADER_VOXELIZATION,
	COMPUTE_SHADER_VOXELIZATION,
	MESH_SHADER_VOXELIZATION
};

class Voxelizer
{
public:

	dw::vk::Image::Ptr			  m_image;
	dw::vk::ImageView::Ptr		  m_image_view;
	const uint32_t m_voxels_per_side;
	glm::vec3 m_AABB_min, m_AABB_max;
	glm::vec3 m_center;
	float m_length;
	const float m_voxel_width;

	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_image;
	dw::vk::DescriptorSet::Ptr       m_ds_image;

	dw::vk::PipelineLayout::Ptr   m_reset_compute_pipeline_layout;
	dw::vk::PipelineLayout::Ptr   m_visualizer_compute_pipeline_layout;
	dw::vk::PipelineLayout::Ptr   m_visualizer_graphics_pipeline_layout;
	dw::vk::ComputePipeline::Ptr  m_reset_compute_pipeline;
	dw::vk::ComputePipeline::Ptr  m_visualizer_compute_pipeline;
	dw::vk::GraphicsPipeline::Ptr m_visualizer_graphics_pipeline;
	dw::vk::Framebuffer::Ptr      m_framebuffer;
	dw::vk::RenderPass::Ptr       m_render_pass;

	size_t							 m_visualizer_ubo_size;
	dw::vk::Buffer::Ptr				 m_visualizer_ubo_data;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_ubo;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_instance_buffer;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_instance_color_buffer;
	dw::vk::DescriptorSetLayout::Ptr m_ds_layout_indirect_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_visualizer_ubo;
	VisualizerUBO					 m_visualizer_transforms;
	RenderObject					 m_cube;

	const VoxelizationType m_voxelization_type;
	bool m_voxelization_visualization_wireframe = false;
	bool first_time = true;

	Voxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, VoxelizationType voxelization_type, uint32_t m_viewport_width, uint32_t m_viewport_height);
	virtual ~Voxelizer();

	virtual void Voxelizer::begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend) = 0;
	virtual void Voxelizer::end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf) = 0;

	void transition_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	int get_work_groups_dim();
	void reset_voxelization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void voxelization_visualization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_instance_buffer(dw::vk::CommandBuffer::Ptr cmd_buf);
	void begin_render_visualizer(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend);
	void render_voxels(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_init_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf);
	void reset_voxelization_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf);
	void visualization_main_buffer_memory_barrier(dw::vk::CommandBuffer::Ptr cmd_buf);
	void create_visualizer_graphics_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_voxel_reset_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_reset_instance_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void create_visualizer_compute_pipeline_state(dw::vk::Backend::Ptr backend);
	void dispatch_visualization_compute_shader(dw::vk::Backend::Ptr backend, dw::vk::CommandBuffer::Ptr cmd_buf);
	AABB get_AABB() const;

protected:
	size_t							 m_ubo_size;
	dw::vk::Buffer::Ptr				 m_ubo_data;

	dw::vk::DescriptorSet::Ptr       m_ds_data;
	VoxelizerData					 m_data;

	float get_length(glm::vec3 AABB_min, glm::vec3 AABB_max) const;
	
private:
	uint32_t m_viewport_width;
	uint32_t m_viewport_height;

	dw::vk::PipelineLayout::Ptr   m_reset_instance_compute_pipeline_layout;
	dw::vk::ComputePipeline::Ptr  m_reset_instance_compute_pipeline;

	size_t							 m_instance_buffer_size;
	dw::vk::Buffer::Ptr				 m_instance_buffer;
	size_t							 m_instance_color_buffer_size;
	dw::vk::Buffer::Ptr				 m_instance_color_buffer;
	size_t							 m_indirect_buffer_size;
	dw::vk::Buffer::Ptr				 m_indirect_buffer;

	dw::vk::DescriptorSet::Ptr       m_ds_instance_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_instance_color_buffer;
	dw::vk::DescriptorSet::Ptr       m_ds_indirect_buffer;

	void create_descriptor_sets(dw::vk::Backend::Ptr backend);
	void create_pipelines(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state);

};