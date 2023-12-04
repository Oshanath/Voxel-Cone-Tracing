#include "GeometryVoxelizer.h"

GeometryVoxelizer::GeometryVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height) :
    Voxelizer(backend, AABB_min, AABB_max, voxels_per_side, vertex_input_state, GEOMETRY_SHADER_VOXELIZATION, m_viewport_width, m_viewport_height),
    m_cam_pos(m_center + glm::vec3(0.0f, 0.0f, m_length / 2)),
    m_cam_forward(glm::vec3(0.0f, 0.0f, -1.0f)),
    m_view(glm::lookAt(m_cam_pos, m_center, glm::vec3(0.0f, 1.0f, 0.0f))),
    m_proj(glm::ortho(-m_length / 2, m_length / 2, -m_length / 2, m_length / 2, -m_length, m_length))
{

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
}

GeometryVoxelizer::~GeometryVoxelizer()
{
    m_pipeline.reset();
    m_pipeline_layout.reset();
}

void GeometryVoxelizer::create_descriptor_sets(dw::vk::Backend::Ptr backend)
{
    m_ubo_size = backend->aligned_dynamic_ubo_size(sizeof(VoxelizerData));
    m_ubo_data = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    // -------------------------------------------------------------------

    VkDescriptorBufferInfo buffer_info;
    VkWriteDescriptorSet   write_data;

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
}

glm::vec3 GeometryVoxelizer::get_center() const
{
    return m_center;
}

glm::mat4 GeometryVoxelizer::get_view() const
{
    return m_view;
}

glm::mat4 GeometryVoxelizer::get_proj() const
{
    return m_proj;
}

glm::vec3 GeometryVoxelizer::get_cam_pos() const
{
    return m_cam_pos;
}

uint32_t GeometryVoxelizer::get_voxels_per_side() const
{
    return m_voxels_per_side;
}

void GeometryVoxelizer::begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
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

    AABB aabb       = get_AABB();
    m_data.AABB_min = glm::vec4(aabb.min, 1.0f);
    m_data.AABB_max = glm::vec4(aabb.max, 1.0f);

    uint8_t* ptr = (uint8_t*)m_ubo_data->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_data, sizeof(VoxelizerData));

    uint32_t dynamic_offset = m_ubo_size * backend->current_frame_idx();
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout->handle(), 1, 1, &m_ds_data->handle(), 1, &dynamic_offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout->handle(), 2, 1, &m_ds_image->handle(), 0, nullptr);
}

void GeometryVoxelizer::end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf->handle());
}

void GeometryVoxelizer::create_voxelization_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state)
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(backend, "shaders/geometry_voxelizer.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(backend, "shaders/geometry_voxelizer.frag.spv");
    dw::vk::ShaderModule::Ptr gs = dw::vk::ShaderModule::create_from_file(backend, "shaders/geometry_voxelizer.geom.spv");

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

    rs_state.set_conservative_raster_mode(VK_CONSERVATIVE_RASTERIZATION_MODE_OVERESTIMATE_EXT);

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
        .add_descriptor_set_layout(m_ds_layout_ubo_dynamic)
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