#include "VCTRenderer.h"
#include <array>

bool Sample::init(int argc, const char* argv[])
{
    // Create GPU resources.
    if (!create_shaders())
        return false;

    if (!create_uniform_buffer())
        return false;

    // Load mesh.
    if (!load_objects())
        return false;

    // Shadow map
    m_shadow_map = std::make_unique<dw::ShadowMap>(m_vk_backend, m_shadow_map_size);

    // Shadow map sampler
    dw::vk::Sampler::Desc sampler_desc;
    DW_ZERO_MEMORY(sampler_desc);
    sampler_desc.mag_filter = VK_FILTER_LINEAR;
    sampler_desc.min_filter = VK_FILTER_LINEAR;
    sampler_desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sampler_desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.anisotropy_enable = VK_FALSE;
    sampler_desc.compare_enable = VK_TRUE;
    sampler_desc.compare_op = VK_COMPARE_OP_LESS;
    m_shadow_map_sampler = dw::vk::Sampler::create(m_vk_backend, sampler_desc);

    create_descriptor_set_layouts();
    create_descriptor_sets();
    write_descriptor_sets();
    create_main_pipeline_state();
    create_shadow_pipeline_state();

    // Create camera.
    create_camera();

    return true;
}

void Sample::update(double delta)
{
    dw::vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

    VkCommandBufferBeginInfo begin_info;
    DW_ZERO_MEMORY(begin_info);

    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

    {
        DW_SCOPED_SAMPLE("update", cmd_buf);

        // Render profiler.
        dw::profiler::ui();

        // Update camera.
        update_camera();

        // Update uniforms.
        update_uniforms(cmd_buf);

        // Render.
        render(cmd_buf);
    }

    vkEndCommandBuffer(cmd_buf->handle());

    submit_and_present({ cmd_buf });
}

void Sample::shutdown()
{
    for (auto& mesh : m_meshes)
        mesh.reset();
    for (auto& object : objects)
        object.reset();
    m_graphics_pipeline_main.reset();
    m_graphics_pipeline_shadow.reset();
    m_pipeline_layout_main.reset();
    m_pipeline_layout_shadow.reset();
    m_per_frame_ds_layout.reset();
    m_shadow_ds_layout.reset();
    m_per_frame_ds.reset();
    m_shadow_ds.reset();
    m_ubo.reset();
    m_shadow_map.reset();
    m_shadow_map_sampler.reset();
}

dw::AppSettings Sample::intial_app_settings()
{
    // Set custom settings here...
    dw::AppSettings settings;

    settings.width       = 1280;
    settings.height      = 720;
    settings.title       = "Voxel Cone Tracing Demo (Vulkan)";
    settings.ray_tracing = false;
    settings.fullscreen  = false;

    return settings;
}

bool Sample::create_uniform_buffer()
{
    m_ubo_size = m_vk_backend->aligned_dynamic_ubo_size(sizeof(Transforms));
    m_ubo      = dw::vk::Buffer::create(m_vk_backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    return true;
}

void Sample::create_descriptor_set_layouts()
{
    // main 
    dw::vk::DescriptorSetLayout::Desc desc;
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT);
    m_per_frame_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);

    // shadow
    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_shadow_ds_layout = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
}

void Sample::write_descriptor_sets()
{
    VkDescriptorBufferInfo buffer_info;

    buffer_info.buffer = m_ubo->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(Transforms);

    VkWriteDescriptorSet write_data;
    DW_ZERO_MEMORY(write_data);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_per_frame_ds->handle();

    vkUpdateDescriptorSets(m_vk_backend->device(), 1, &write_data, 0, nullptr);
}

void Sample::create_main_pipeline_state()
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/mesh.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/mesh.frag.spv");

    dw::vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    pso_desc.set_vertex_input_state(m_meshes[0]->vertex_input_state_desc());

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

    vp_desc.add_viewport(0.0f, 0.0f, m_width, m_height, 0.0f, 1.0f)
        .add_scissor(0, 0, m_width, m_height);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    dw::vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT)
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

    ds_state.set_depth_test_enable(VK_TRUE)
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

    pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout)
        .add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(m_shadow_ds_layout);
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants));

    m_pipeline_layout_main = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);

    pso_desc.set_pipeline_layout(m_pipeline_layout_main);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    // ---------------------------------------------------------------------------
    // Create pipeline
    // ---------------------------------------------------------------------------

    pso_desc.set_render_pass(m_vk_backend->swapchain_render_pass());

    m_graphics_pipeline_main = dw::vk::GraphicsPipeline::create(m_vk_backend, pso_desc);
}

void Sample::create_shadow_pipeline_state()
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/shadow.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(m_vk_backend, "shaders/shadow.frag.spv");

    dw::vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
        .add_shader_stage(VK_SHADER_STAGE_FRAGMENT_BIT, fs, "main");

    // ---------------------------------------------------------------------------
    // Create vertex input state
    // ---------------------------------------------------------------------------

    pso_desc.set_vertex_input_state(m_meshes[0]->vertex_input_state_desc());

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

    vp_desc.add_viewport(0.0f, 0.0f, m_shadow_map_size, m_shadow_map_size, 0.0f, 1.0f)
        .add_scissor(0, 0, m_shadow_map_size, m_shadow_map_size);

    pso_desc.set_viewport_state(vp_desc);

    // ---------------------------------------------------------------------------
    // Create rasterization state
    // ---------------------------------------------------------------------------

    dw::vk::RasterizationStateDesc rs_state;

    rs_state.set_depth_clamp(VK_FALSE)
        .set_rasterizer_discard_enable(VK_FALSE)
        .set_polygon_mode(VK_POLYGON_MODE_FILL)
        .set_line_width(1.0f)
        .set_cull_mode(VK_CULL_MODE_BACK_BIT)
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

    ds_state.set_depth_test_enable(VK_TRUE)
        .set_depth_write_enable(VK_TRUE)
        .set_depth_compare_op(VK_COMPARE_OP_LESS)
        .set_depth_bounds_test_enable(VK_FALSE)
        .set_stencil_test_enable(VK_FALSE);

    pso_desc.set_depth_stencil_state(ds_state);

    // ---------------------------------------------------------------------------
    // Create color blend state
    // ---------------------------------------------------------------------------

    dw::vk::ColorBlendAttachmentStateDesc blend_att_desc;

    blend_att_desc.set_color_write_mask(0)
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

    pl_desc.add_descriptor_set_layout(m_per_frame_ds_layout)
        .add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(m_shadow_ds_layout)
        .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants));

    m_pipeline_layout_shadow = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);

    pso_desc.set_pipeline_layout(m_pipeline_layout_shadow);

    // ---------------------------------------------------------------------------
    // Create dynamic state
    // ---------------------------------------------------------------------------

    pso_desc.add_dynamic_state(VK_DYNAMIC_STATE_VIEWPORT)
        .add_dynamic_state(VK_DYNAMIC_STATE_SCISSOR);

    // ---------------------------------------------------------------------------
    // Create pipeline
    // ---------------------------------------------------------------------------

    pso_desc.set_render_pass(m_shadow_map->render_pass());

    m_graphics_pipeline_shadow = dw::vk::GraphicsPipeline::create(m_vk_backend, pso_desc);
}

bool Sample::load_object(std::string filename)
{
    m_meshes.push_back(dw::Mesh::load(m_vk_backend, filename));
    objects.push_back(RenderObject(m_meshes[m_meshes.size() - 1]));
    return m_meshes[m_meshes.size() - 1] != nullptr;
}

bool Sample::load_objects()
{
    std::vector<bool> results;
    results.push_back(load_object("sponza.obj"));
    results.push_back(load_object("teapot.obj"));
    objects[objects.size() - 1].position = glm::vec3(20.0f, 20.0f, 20.0f);

    for (bool result : results) {
        if (!result)
            return false;
    }

    return true;
}

void Sample::render_objects(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::PipelineLayout::Ptr pipeline_layout)
{
    VkDeviceSize offset = 0;

    for (auto object : objects)
    {
        auto mesh = object.mesh;

        vkCmdBindVertexBuffers(cmd_buf->handle(), 0, 1, &mesh->vertex_buffer()->handle(), &offset);
        vkCmdBindIndexBuffer(cmd_buf->handle(), mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

        // push constant

        const auto& submeshes = mesh->sub_meshes();

        for (uint32_t i = 0; i < submeshes.size(); i++)
        {
            auto& submesh = submeshes[i];
            auto& mat     = mesh->material(submesh.mat_idx);

            vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout->handle(), 1, 1, &mat->descriptor_set()->handle(), 0, nullptr);
            vkCmdPushConstants(cmd_buf->handle(), pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), glm::value_ptr(object.get_model()));

            // Issue draw call.
            vkCmdDrawIndexed(cmd_buf->handle(), submesh.index_count, 1, submesh.base_index, submesh.base_vertex, 0);
        }
    }
}

void Sample::begin_render_main(dw::vk::CommandBuffer::Ptr cmd_buf)
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
    info.renderPass               = m_vk_backend->swapchain_render_pass()->handle();
    info.framebuffer              = m_vk_backend->swapchain_framebuffer()->handle();
    info.renderArea.extent.width  = m_width;
    info.renderArea.extent.height = m_height;
    info.clearValueCount          = 2;
    info.pClearValues             = &clear_values[0];

    vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline_main->handle());

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = (float)m_height;
    vp.width    = (float)m_width;
    vp.height   = -(float)m_height;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = m_width;
    scissor_rect.extent.height = m_height;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);
}

void Sample::begin_render_shadow(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    m_shadow_map->begin_render(cmd_buf);
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline_shadow->handle());
}

void Sample::render(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    DW_SCOPED_SAMPLE("render", cmd_buf);

    const uint32_t dynamic_offset = m_ubo_size * m_vk_backend->current_frame_idx();

    begin_render_shadow(cmd_buf);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 0, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_shadow->handle(), 2, 1, &m_shadow_ds->handle(), 0, nullptr);
    render_objects(cmd_buf, m_pipeline_layout_shadow);
    m_shadow_map->end_render(cmd_buf);

    begin_render_main(cmd_buf);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 0, 1, &m_per_frame_ds->handle(), 1, &dynamic_offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_shadow->handle(), 2, 1, &m_shadow_ds->handle(), 0, nullptr);
    render_objects(cmd_buf, m_pipeline_layout_main);

    render_gui(cmd_buf);
    vkCmdEndRenderPass(cmd_buf->handle());
}

void Sample::update_uniforms(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    DW_SCOPED_SAMPLE("update_uniforms", cmd_buf);
    m_transforms.view       = m_main_camera->m_view;
    m_transforms.projection = m_main_camera->m_projection;

    uint8_t* ptr = (uint8_t*)m_ubo->mapped_ptr();
    memcpy(ptr + m_ubo_size * m_vk_backend->current_frame_idx(), &m_transforms, sizeof(Transforms));
}

void Sample::update_camera()
{
    dw::Camera* current = m_main_camera.get();

    float forward_delta = m_heading_speed * m_delta;
    float right_delta   = m_sideways_speed * m_delta;
    float up_delta      = m_climbing_speed * m_delta;

    current->set_translation_delta(current->m_forward, forward_delta);
    current->set_translation_delta(current->m_right, right_delta);
    current->set_translation_delta(current->m_up, up_delta);

    m_camera_x = m_mouse_delta_x * m_camera_sensitivity;
    m_camera_y = m_mouse_delta_y * m_camera_sensitivity;

    if (m_mouse_look)
    {
        // Activate Mouse Look
        current->set_rotatation_delta(glm::vec3((float)(m_camera_y),
                                                (float)(m_camera_x),
                                                (float)(0.0f)));
    }
    else
    {
        current->set_rotatation_delta(glm::vec3((float)(0),
                                                (float)(0),
                                                (float)(0)));
    }

    current->update();
}

DW_DECLARE_MAIN(Sample)
