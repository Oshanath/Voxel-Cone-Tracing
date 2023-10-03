#include "Voxelizer.h"
#include <gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>
#include <algorithm>
#include <material.h>

Voxelizer::Voxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height) :
    m_AABB_min(AABB_min), m_AABB_max(AABB_max),
    m_length(get_length(AABB_min, AABB_max)),
    m_center((AABB_min + AABB_max) / 2.0f),
    m_cam_pos(m_center + glm::vec3(m_length / 2, 0.0f, 0.0f)),
    m_cam_forward(glm::vec3(-1.0f, 0.0f, 0.0f)),
    m_view(glm::lookAt(m_cam_pos, m_center, glm::vec3(0.0f, 1.0f, 0.0f))),
    m_proj(glm::ortho(-m_length/2, m_length/2, -m_length/2, m_length/2, -m_length, m_length)),
    m_voxels_per_side(voxels_per_side),
    m_viewport_width(m_viewport_width),
    m_viewport_height(m_viewport_height),
    m_voxel_width(m_length / m_voxels_per_side),
    m_cube(RenderObject(dw::Mesh::load(backend, "cube.obj")))
{

    VkFormat voxel_grid_format = VK_FORMAT_R8G8B8A8_UNORM;

    m_image      = dw::vk::Image::create(backend, VK_IMAGE_TYPE_3D, m_voxels_per_side, m_voxels_per_side, m_voxels_per_side, 1, 1, voxel_grid_format, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_STORAGE_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
    m_image_view = dw::vk::ImageView::create(backend, m_image, VK_IMAGE_VIEW_TYPE_3D, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1);

    std::vector<VkSubpassDescription> subpass_description(1);

    subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description[0].colorAttachmentCount    = 0;
    subpass_description[0].pColorAttachments       = nullptr;
    subpass_description[0].pDepthStencilAttachment = nullptr;
    subpass_description[0].inputAttachmentCount    = 0;
    subpass_description[0].pInputAttachments       = nullptr;
    subpass_description[0].preserveAttachmentCount = 0;
    subpass_description[0].pPreserveAttachments    = nullptr;
    subpass_description[0].pResolveAttachments     = nullptr;

    m_render_pass = dw::vk::RenderPass::create(backend, {}, subpass_description, {});
    m_framebuffer = dw::vk::Framebuffer::create(backend, m_render_pass, {}, m_voxels_per_side, m_voxels_per_side, m_voxels_per_side);

    create_descriptor_sets(backend);
    create_voxelization_pipeline_state(backend, vertex_input_state);
    create_voxel_reset_compute_pipeline_state(backend);
    create_reset_instance_compute_pipeline_state(backend);
    create_visualizer_compute_pipeline_state(backend);
    create_visualizer_graphics_pipeline_state(backend);

    VkDrawIndexedIndirectCommand indirect_command;
    indirect_command.instanceCount = 1;
    indirect_command.firstInstance = 0;
    indirect_command.indexCount    = 36;
    indirect_command.firstIndex    = 0;
    indirect_command.vertexOffset  = 0;

    uint8_t* ptr = (uint8_t*)m_indirect_buffer->mapped_ptr();
    memcpy(ptr, &indirect_command, sizeof(VkDrawIndexedIndirectCommand));

    float cos45  = glm::cos(glm::radians(45.0f));
    m_cube.scale = m_voxel_width / (2 * cos45 * cos45);
}

Voxelizer::~Voxelizer()
{
    //m_framebuffer.reset();
    m_render_pass.reset();
    m_image_view.reset();
    m_image.reset();
    m_pipeline.reset();
    m_ubo_data.reset();
    m_ds_data.reset();
    m_ds_image.reset();
    m_ds_layout_image.reset();
    m_ds_layout_ubo.reset();
    m_pipeline_layout.reset();
}

float Voxelizer::get_length(glm::vec3 AABB_min, glm::vec3 AABB_max) const
{
    float x_length = std::abs(AABB_min.x - AABB_max.x);
    float y_length = std::abs(AABB_min.y - AABB_max.y);
    float z_length = std::abs(AABB_min.z - AABB_max.z);

    return max(x_length, max(y_length, z_length));
}

void Voxelizer::create_descriptor_sets(dw::vk::Backend::Ptr backend)
{
    m_ubo_size = backend->aligned_dynamic_ubo_size(sizeof(VoxelizerData));
    m_ubo_data = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    m_instance_buffer_size = sizeof(InstanceData) * m_voxels_per_side * m_voxels_per_side * m_voxels_per_side;
    m_instance_buffer      = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_instance_buffer_size, VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    m_indirect_buffer_size = backend->aligned_dynamic_ubo_size(sizeof(VkDrawIndirectCommand));
    m_indirect_buffer      = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_indirect_buffer_size, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    m_visualizer_ubo_size = backend->aligned_dynamic_ubo_size(sizeof(VoxelizerData));
    m_visualizer_ubo_data = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_visualizer_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    dw::vk::DescriptorSetLayout::Desc desc;
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_ubo = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_ubo->set_name("Voxelizer::m_ds_layout_ubo");

    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_image = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_image->set_name("Voxelizer::m_ds_layout_image");

    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_instance_buffer = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_instance_buffer->set_name("Voxelizer::m_ds_layout_instance_buffer");

    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_indirect_buffer = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_indirect_buffer->set_name("Voxelizer::m_ds_layout_indirect_buffer");

    m_ds_data  = backend->allocate_descriptor_set(m_ds_layout_ubo);
    m_ds_image = backend->allocate_descriptor_set(m_ds_layout_image);
    m_ds_instance_buffer = backend->allocate_descriptor_set(m_ds_layout_instance_buffer);
    m_ds_indirect_buffer = backend->allocate_descriptor_set(m_ds_layout_indirect_buffer);
    m_ds_visualizer_ubo  = backend->allocate_descriptor_set(m_ds_layout_ubo);

    // -------------------------------------------------------------------

    VkDescriptorBufferInfo buffer_info;
    VkWriteDescriptorSet   write_data;
    VkDescriptorImageInfo  image_info;

    // UBO Transforms
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_ubo_data->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(VoxelizerData);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_data->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    // Visualizer UBO Transforms
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_visualizer_ubo_data->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(VisualizerUBO);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_visualizer_ubo->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    // SSBO instance buffer
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_instance_buffer->handle();
    buffer_info.offset = 0;
    buffer_info.range  = m_instance_buffer_size;

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_instance_buffer->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    // SSBO indirect buffer
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_indirect_buffer->handle();
    buffer_info.offset = 0;
    buffer_info.range  = m_indirect_buffer_size;

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_indirect_buffer->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    // 3D texture

    // Voxelizer 3D image
    DW_ZERO_MEMORY(write_data);
    DW_ZERO_MEMORY(image_info);

    image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    image_info.imageView   = m_image_view->handle();
    image_info.sampler     = nullptr;

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write_data.pImageInfo      = &image_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_image->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);
}

void Voxelizer::transition_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = m_image->handle();
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Voxelizer::reset_voxelization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = m_image->handle();
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask                   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Voxelizer::reset_init_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkBufferMemoryBarrier barrier = {};
    barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer                = m_indirect_buffer->handle();
    barrier.srcAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.size                  = m_indirect_buffer_size;
    barrier.offset                = 0;
    barrier.pNext                 = nullptr;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &barrier, 0, nullptr);
}

void Voxelizer::reset_voxelization_buffer_memory_barrier_indirect(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkBufferMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer                           = m_indirect_buffer->handle();
    barrier.srcAccessMask                    = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask                    = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.size                             = m_indirect_buffer_size;
    barrier.offset                           = 0;
    barrier.pNext                            = nullptr;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &barrier, 0, nullptr);
}

void Voxelizer::voxelization_visualization_image_memory_barrier_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkImageMemoryBarrier barrier            = {};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image                           = m_image->handle();
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask                   = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask                   = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    
    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 1, &barrier);
}

void Voxelizer::visualization_main_buffer_memory_barrier(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkBufferMemoryBarrier barrier = {};
    barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer                = m_instance_buffer->handle();
    barrier.srcAccessMask         = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.size                  = m_instance_buffer_size;
    barrier.offset                = 0;
    barrier.pNext                 = nullptr;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &barrier, 0, nullptr);
}

glm::vec3 Voxelizer::get_center() const
{
    return m_center;
}

glm::mat4 Voxelizer::get_view() const
{
    return m_view;
}

glm::mat4 Voxelizer::get_proj() const
{
    return m_proj;
}

glm::vec3 Voxelizer::get_cam_pos() const
{
    return m_cam_pos;
}

AABB Voxelizer::get_AABB() const
{
    glm::vec3 min = m_center - glm::vec3(m_length / 2, m_length / 2, m_length / 2);
    glm::vec3 max = m_center + glm::vec3(m_length / 2, m_length / 2, m_length / 2);
    return AABB { min, max };
}

uint32_t Voxelizer::get_voxels_per_side() const
{
    return m_voxels_per_side;
}

void Voxelizer::begin_render(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = m_render_pass->handle();
    info.framebuffer              = m_framebuffer->handle();
    info.renderArea.extent.width  = m_voxels_per_side;
    info.renderArea.extent.height = m_voxels_per_side;
    info.clearValueCount          = 0;
    info.pClearValues             = nullptr;

    vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->handle());

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)m_voxels_per_side;
    vp.height   = (float)m_voxels_per_side;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = m_voxels_per_side;
    scissor_rect.extent.height = m_voxels_per_side;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

    m_data.view       = get_view();
    m_data.projection = get_proj();

    AABB aabb                 = get_AABB();
    m_data.AABB_min = glm::vec4(aabb.min, float(get_voxels_per_side()));
    m_data.AABB_max = aabb.max;

    uint8_t* ptr = (uint8_t*)m_ubo_data->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_data, sizeof(VoxelizerData));

    uint32_t dynamic_offset = m_ubo_size * backend->current_frame_idx();
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout->handle(), 1, 1, &m_ds_data->handle(), 1, &dynamic_offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout->handle(), 2, 1, &m_ds_image->handle(), 0, nullptr);
}

void Voxelizer::end_render(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf->handle());
}

void Voxelizer::create_voxelization_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state)
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel.frag.spv");
    dw::vk::ShaderModule::Ptr gs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel.geom.spv");

    dw::vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
            .add_shader_stage(VK_SHADER_STAGE_GEOMETRY_BIT, gs, "main")
            .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    pso_desc.set_vertex_input_state(vertex_input_state);

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    dw::vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false)
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    dw::vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, m_voxels_per_side, m_voxels_per_side, 0.0f, 1.0f)
        .add_scissor(0, 0, m_voxels_per_side, m_voxels_per_side);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    dw::vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_NONE)
        .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_depth_bias(VK_FALSE);

    pso_desc.set_rasterization_state(rs_state);

    // ---------------------------------------------------------------------------
    // Create multisample state
    // ---------------------------------------------------------------------------

    dw::vk::MultisampleStateDesc ms_state;

    ms_state.set_sample_shading_enable(VK_FALSE)
        .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

    pso_desc.set_multisample_state(ms_state);

    // ---------------------------------------------------------------------------
    // Create depth stencil state
    // ---------------------------------------------------------------------------

    dw::vk::DepthStencilStateDesc ds_state;

    ds_state.set_depth_test_enable(VK_FALSE)
        .set_depth_write_enable(VK_TRUE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    dw::vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        .set_blend_enable(VK_FALSE);

    dw::vk::ColorBlendStateDesc blend_state;

    blend_state.set_logic_op_enable(VK_FALSE)
        .set_logic_op(VK_LOGIC_OP_COPY)
        .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
        .add_attachment(blend_att_desc);

    pso_desc.set_color_blend_state(blend_state);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    dw::vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(m_ds_layout_ubo)
        .add_descriptor_set_layout(m_ds_layout_image);
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants));

    m_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    // ---------------------------------------------------------------------------
    // Create pipeline
    // ---------------------------------------------------------------------------

    pso_desc.set_render_pass(m_render_pass);

    m_pipeline = dw::vk::GraphicsPipeline::create(backend, pso_desc);
}

void Voxelizer::create_voxel_reset_compute_pipeline_state(dw::vk::Backend::Ptr backend)
{
    dw::vk::ShaderModule::Ptr cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/reset.comp.spv");
    dw::vk::ComputePipeline::Desc  pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_image)
        .add_descriptor_set_layout(m_ds_layout_indirect_buffer);
    m_reset_compute_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_reset_compute_pipeline_layout);
    m_reset_compute_pipeline = dw::vk::ComputePipeline::create(backend, pso_desc);
}

void Voxelizer::create_reset_instance_compute_pipeline_state(dw::vk::Backend::Ptr backend)
{
    dw::vk::ShaderModule::Ptr     cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/reset_instance.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_indirect_buffer);
    m_reset_instance_compute_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_reset_instance_compute_pipeline_layout);
    m_reset_instance_compute_pipeline = dw::vk::ComputePipeline::create(backend, pso_desc);
}

void Voxelizer::create_visualizer_compute_pipeline_state(dw::vk::Backend::Ptr backend)
{
    dw::vk::ShaderModule::Ptr     cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel_vis.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_image)
        .add_descriptor_set_layout(m_ds_layout_instance_buffer)
        .add_descriptor_set_layout(m_ds_layout_indirect_buffer)
        .add_descriptor_set_layout(m_ds_layout_ubo);
    m_visualizer_compute_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);
    m_visualizer_compute_pipeline_layout->set_name("Voxelizer::m_visualizer_compute_pipeline_layout");

    pso_desc.set_pipeline_layout(m_visualizer_compute_pipeline_layout);
    m_visualizer_compute_pipeline = dw::vk::ComputePipeline::create(backend, pso_desc);
    m_visualizer_compute_pipeline->set_name("Voxelizer::m_visualizer_compute_pipeline");
}

void Voxelizer::create_visualizer_graphics_pipeline_state(dw::vk::Backend::Ptr backend)
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel_vis.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(backend, "shaders/voxel_vis.frag.spv");

    dw::vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    pso_desc.set_vertex_input_state(m_cube.mesh->vertex_input_state_desc());

    // ---------------------------------------------------------------------------
    // Create pipeline input assembly state
    // ---------------------------------------------------------------------------

    dw::vk::InputAssemblyStateDesc input_assembly_state_desc;

    input_assembly_state_desc.set_primitive_restart_enable(false)
        .set_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pso_desc.set_input_assembly_state(input_assembly_state_desc);

    // ---------------------------------------------------------------------------
    // Create viewport state
    // ---------------------------------------------------------------------------

    dw::vk::ViewportStateDesc vp_desc;

    vp_desc.add_viewport(0.0f, 0.0f, m_viewport_width, m_viewport_height, 0.0f, 1.0f)
        .add_scissor(0, 0, m_viewport_width, m_viewport_height);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    dw::vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_NONE)
        .set_front_face(VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .set_depth_bias(VK_FALSE);

    if (m_voxelization_visualization_wireframe)
        rs_state.set_polygon_mode(VK_POLYGON_MODE_LINE);
    else
        rs_state.set_polygon_mode(VK_POLYGON_MODE_FILL);

    pso_desc.set_rasterization_state(rs_state);

    // ---------------------------------------------------------------------------
    // Create multisample state
    // ---------------------------------------------------------------------------

    dw::vk::MultisampleStateDesc ms_state;

    ms_state.set_sample_shading_enable(VK_FALSE)
        .set_rasterization_samples(VK_SAMPLE_COUNT_1_BIT);

    pso_desc.set_multisample_state(ms_state);

    // ---------------------------------------------------------------------------
    // Create depth stencil state
    // ---------------------------------------------------------------------------

    dw::vk::DepthStencilStateDesc ds_state;

    ds_state.set_depth_test_enable(VK_TRUE)
        .set_depth_write_enable(VK_FALSE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    dw::vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT)
        .set_blend_enable(VK_FALSE);

    dw::vk::ColorBlendStateDesc blend_state;

    blend_state.set_logic_op_enable(VK_FALSE)
        .set_logic_op(VK_LOGIC_OP_COPY)
        .set_blend_constants(0.0f, 0.0f, 0.0f, 0.0f)
        .add_attachment(blend_att_desc);

    pso_desc.set_color_blend_state(blend_state);

    // ---------------------------------------------------------------------------
    // Create pipeline layout
    // ---------------------------------------------------------------------------

    dw::vk::PipelineLayout::Desc pl_desc;

    pl_desc.add_descriptor_set_layout(m_ds_layout_ubo)
        .add_descriptor_set_layout(m_ds_layout_instance_buffer);

    m_visualizer_graphics_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_visualizer_graphics_pipeline_layout);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    // ---------------------------------------------------------------------------
    // Create pipeline
    // ---------------------------------------------------------------------------

    pso_desc.set_render_pass(backend->swapchain_render_pass());

    m_visualizer_graphics_pipeline = dw::vk::GraphicsPipeline::create(backend, pso_desc);
}

void Voxelizer::reset_voxel_grid(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    // Compute
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_reset_compute_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_reset_compute_pipeline_layout->handle(), 0, 1, &m_ds_image->handle(), 0, nullptr);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_reset_compute_pipeline_layout->handle(), 1, 1, &m_ds_indirect_buffer->handle(), 0, nullptr);
    vkCmdDispatch(cmd_buf->handle(), 8, 8, 8);
}

void Voxelizer::reset_instance_buffer(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    // Compute
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_reset_instance_compute_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_reset_instance_compute_pipeline_layout->handle(), 0, 1, &m_ds_indirect_buffer->handle(), 0, nullptr);
    vkCmdDispatch(cmd_buf->handle(), 1, 1, 1);
}

void Voxelizer::begin_render_visualizer(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{
    VkClearValue clear_values[2];

    clear_values[0].color.float32[0] = 0.0f;
    clear_values[0].color.float32[1] = 0.0f;
    clear_values[0].color.float32[2] = 0.0f;
    clear_values[0].color.float32[3] = 1.0f;

    clear_values[1].color.float32[0] = 1.0f;
    clear_values[1].color.float32[1] = 1.0f;
    clear_values[1].color.float32[2] = 1.0f;
    clear_values[1].color.float32[3] = 1.0f;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = backend->swapchain_render_pass()->handle();
    info.framebuffer              = backend->swapchain_framebuffer()->handle();
    info.renderArea.extent.width  = m_viewport_width;
    info.renderArea.extent.height = m_viewport_height;
    info.clearValueCount          = 2;
    info.pClearValues             = &clear_values[0];

    vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_visualizer_graphics_pipeline->handle());

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)m_viewport_height;
    vp.width    = (float)m_viewport_width;
    vp.height   = -(float)m_viewport_height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = m_viewport_width;
    scissor_rect.extent.height = m_viewport_height;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);
}

void Voxelizer::render_voxels(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkDeviceSize offset = 0;

    auto mesh = m_cube.mesh;

    vkCmdBindVertexBuffers(cmd_buf->handle(), 0, 1, &mesh->vertex_buffer()->handle(), &offset);
    vkCmdBindIndexBuffer(cmd_buf->handle(), mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

    const auto& submeshes = mesh->sub_meshes();

    for (uint32_t i = 0; i < submeshes.size(); i++)
    {
        auto& submesh = submeshes[i];
        auto& mat     = mesh->material(submesh.mat_idx);

        // Issue draw call.
        //vkCmdDrawIndexed(cmd_buf->handle(), submesh.index_count, m_cube_positions.size(), submesh.base_index, submesh.base_vertex, 0);
        //vkCmdDrawIndexed(cmd_buf->handle(), submesh.index_count, m_cube_positions.size(), submesh.base_index, submesh.base_vertex, 0);
        vkCmdDrawIndexedIndirect(cmd_buf->handle(), m_indirect_buffer->handle(), 0, 1, 0);
    }
}
