#include "VCTRenderer.h"
#include <array>

void VCTRenderer::create_voxelizer()
{
    if (m_voxelization_type == COMPUTE_SHADER_VOXELIZATION)
    {
        m_voxelizer = std::make_shared<ComputeVoxelizer>(
            m_vk_backend,
            glm::vec3(-1963.12f, -160.925f, 1119.94f),
            glm::vec3(1950.5f, 1543.24f, -1285.63f),
            m_voxelization_resolution,
            m_meshes[0]->vertex_input_state_desc(),
            m_width,
            m_height,
            objects);
    }
    else if (m_voxelization_type == GEOMETRY_SHADER_VOXELIZATION)
	{
        m_voxelizer = std::make_shared<GeometryVoxelizer>(
            m_vk_backend,
            glm::vec3(-1963.12f, -160.925f, 1119.94f),
            glm::vec3(1950.5f, 1543.24f, -1285.63f),
            m_voxelization_resolution,
            m_meshes[0]->vertex_input_state_desc(),
            m_width,
            m_height);
	}
}

bool VCTRenderer::init(int argc, const char* argv[])
{
    // Create Uniform buffers
    if (!create_uniform_buffers())
        return false;

    // Load mesh.
    RenderObject::initialize_common_resources(m_vk_backend);
    if (!load_objects())
        return false;

    create_descriptor_set_layouts();

    // Shadow map
    m_shadow_map = std::make_unique<ShadowMap>(m_vk_backend, m_shadow_map_size, m_meshes[0]->vertex_input_state_desc());
    m_shadow_map->set_target(glm::vec3(-110.0f, 64.0f, 0.0f));
    m_shadow_map->set_direction(glm::normalize(m_lights.lights[0].direction));
    m_shadow_map->set_backoff_distance(6000.0f);
    m_shadow_map->set_extents(1400.0f);
    m_shadow_map->set_near_plane(1.0f);
    m_shadow_map->set_far_plane(8000.0f);

    create_voxelizer();

    create_descriptor_sets();
    write_descriptor_sets();
    create_main_pipeline_state();

    // Lights
    Light light;
    light.color        = glm::vec3(0.1f, -1.0f, 1.0f);
    light.intensity    = 1.0f;
    light.position     = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    light.direction    = glm::normalize(glm::vec3(0.1f, -1.0f, 0.0f));
    light.type         = 0;
    m_lights.lights[0] = light;

    // Create camera.
    create_camera();

    // Debug draw
    m_debug_draw.init(m_vk_backend, m_vk_backend->swapchain_render_pass());
    m_debug_draw.set_depth_test(true);

    m_compute_fences = std::vector<dw::vk::Fence::Ptr>(m_vk_backend->kMaxFramesInFlight);
    for (auto& fence : m_compute_fences)
		fence = dw::vk::Fence::create(m_vk_backend);

    m_mesh_push_constants.occlusionDecayFactor          = 0.0f;
    m_mesh_push_constants.ambientOcclusionEnabled   = VK_FALSE;
    m_mesh_push_constants.occlusionVisualizationEnabled = VK_FALSE;
    m_mesh_push_constants.surfaceOffset                 = 15.719f;
    m_mesh_push_constants.coneCutoff                    = 143.813f;
    m_mesh_push_constants.noTexture                     = false;
    m_voxelizer->noTexture = m_mesh_push_constants.noTexture;

    return true;
}

void VCTRenderer::update(double delta)
{
    dw::vk::CommandBuffer::Ptr cmd_buf = m_vk_backend->allocate_graphics_command_buffer();

    VkCommandBufferBeginInfo begin_info;
    DW_ZERO_MEMORY(begin_info);
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (ImGui::Checkbox("No Texture", (bool*)(&m_mesh_push_constants.noTexture)))
    {
        m_voxelizer->noTexture = m_mesh_push_constants.noTexture;
    }

    ImGui::Checkbox("Voxelization Visualization", &m_voxelization_visualization_enabled);

    static int res_group = 0;

    if (m_voxelization_resolution == 64)
                res_group = 0;
    else if (m_voxelization_resolution == 128)
                res_group = 1;
    else if (m_voxelization_resolution == 256)
                res_group = 2;
    else if (m_voxelization_resolution == 512)
                res_group = 3;

    ImGui::Text("\nVoxelization resolution");
    if (ImGui::RadioButton("64", &res_group, 0))
        revoxelize(64);
    if (ImGui::RadioButton("128", &res_group, 1))
        revoxelize(128);
    if (ImGui::RadioButton("256", &res_group, 2))
        revoxelize(256);
    if (ImGui::RadioButton("512", &res_group, 3))
        revoxelize(512);

    static int type_group = 1;

    if (m_voxelization_type == COMPUTE_SHADER_VOXELIZATION)
                type_group = 1;
    else if (m_voxelization_type == GEOMETRY_SHADER_VOXELIZATION)
                type_group = 0;

    ImGui::Text("\nVoxelization type");
    if (ImGui::RadioButton("Geometry", &type_group, 0))
    {
        revoxelize(GEOMETRY_SHADER_VOXELIZATION);
    }
    if (ImGui::RadioButton("Compute", &type_group, 1))
    {
        revoxelize(COMPUTE_SHADER_VOXELIZATION);
    }

    if (m_voxelizer->m_voxelization_type == COMPUTE_SHADER_VOXELIZATION)
    {
        ComputeVoxelizer* voxelization_ptr = dynamic_cast<ComputeVoxelizer*>(m_voxelizer.get());
        ImGui::InputInt("Large Triangle Threshold", &voxelization_ptr->m_push_constants.large_triangel_threshold);
    }

    ImGui::Text("");
    ImGui::Checkbox("Enable Ambient Occlusion", (bool*)&m_mesh_push_constants.ambientOcclusionEnabled);
    ImGui::Checkbox("Enable Occlusion Visualization", (bool*)&m_mesh_push_constants.occlusionVisualizationEnabled);
    ImGui::SliderFloat("Occlusion Decay Factor", &m_mesh_push_constants.occlusionDecayFactor, 0.0f, 1.0f);
    ImGui::SliderFloat("Surface Offset", &m_mesh_push_constants.surfaceOffset, 0.0f, 30.0f);
    ImGui::SliderFloat("Cone Cutoff", &m_mesh_push_constants.coneCutoff, 0.0f, 2000.0f);

    vkBeginCommandBuffer(cmd_buf->handle(), &begin_info);

    {
        DW_SCOPED_SAMPLE("update", cmd_buf);

        // Render profiler.
        dw::profiler::ui();

        // Update camera.
        update_camera();

        // Render.
        render(cmd_buf);
    }

    vkEndCommandBuffer(cmd_buf->handle());

    submit_and_present({ cmd_buf });
}

void VCTRenderer::shutdown()
{
    vkDeviceWaitIdle(m_vk_backend->device());

    for (auto& mesh : m_meshes)
        mesh.reset();
    for (auto& object : objects)
        object.reset();
    for (auto& fence : m_compute_fences)
        fence.reset();
    m_graphics_pipeline_main.reset();
    m_pipeline_layout_main.reset();
    m_ds_layout_ubo.reset();
    m_ds_layout_voxel_grid_main.reset();
    m_ds_transforms_main.reset();
    m_ds_voxel_grid_main.reset();
    m_ds_lights.reset();
    m_ubo_transforms_main.reset();
    m_ubo_lights.reset();
    m_ubo_voxel_grid.reset();
    m_shadow_map.reset();
    m_voxelizer.reset();
    m_debug_draw.shutdown();
}

dw::AppSettings VCTRenderer::intial_app_settings()
{
    // Set custom settings here...
    dw::AppSettings settings;

    settings.width       = 1920;
    settings.height      = 1080;
    settings.title       = "Voxel Cone Tracing Demo (Vulkan)";
    settings.ray_tracing = false;
    settings.fullscreen  = false;

    return settings;
}

inline void VCTRenderer::window_resized(int width, int height)
{
    // Override window resized method to update camera projection.
    m_main_camera->update_projection(60.0f, 0.1f, m_far, float(m_width) / float(m_height));
}

bool VCTRenderer::create_uniform_buffers()
{
    m_ubo_size_main         = m_vk_backend->aligned_dynamic_ubo_size(sizeof(TransformsMain));
    m_ubo_size_lights       = m_vk_backend->aligned_dynamic_ubo_size(sizeof(Lights));
    m_ubo_size_voxel_grid   = m_vk_backend->aligned_dynamic_ubo_size(sizeof(VoxelizerData));

    m_ubo_transforms_main   = dw::vk::Buffer::create(m_vk_backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size_main * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_ubo_lights            = dw::vk::Buffer::create(m_vk_backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size_lights * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_ubo_voxel_grid        = dw::vk::Buffer::create(m_vk_backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size_voxel_grid * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    return true;
}

void VCTRenderer::create_descriptor_set_layouts()
{
    // main 
    dw::vk::DescriptorSetLayout::Desc desc;
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    m_ds_layout_ubo = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
    m_ds_layout_ubo->set_name("Main::ds_layout_ubo");

    // voxel grid
    desc = {};
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	m_ds_layout_voxel_grid_main = dw::vk::DescriptorSetLayout::create(m_vk_backend, desc);
	m_ds_layout_voxel_grid_main->set_name("Main::ds_layout_voxel_grid");

}

inline void VCTRenderer::create_descriptor_sets()
{
    m_ds_transforms_main   = m_vk_backend->allocate_descriptor_set(m_ds_layout_ubo);
    m_ds_transforms_main->set_name("Main::ds_transforms_main");

    m_ds_lights            = m_vk_backend->allocate_descriptor_set(m_ds_layout_ubo);
    m_ds_lights->set_name("Main::ds_lights");

    m_ds_voxel_grid_main = m_vk_backend->allocate_descriptor_set(m_ds_layout_voxel_grid_main);
    m_ds_voxel_grid_main->set_name("Main::ds_voxel_grid_main");
}

void VCTRenderer::write_descriptor_sets()
{
    // main uniform buffer
    VkDescriptorBufferInfo buffer_info;

    buffer_info.buffer = m_ubo_transforms_main->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(TransformsMain);

    VkWriteDescriptorSet write_data;
    DW_ZERO_MEMORY(write_data);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_transforms_main->handle();

    vkUpdateDescriptorSets(m_vk_backend->device(), 1, &write_data, 0, nullptr);

    // Lights uniform buffer
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_ubo_lights->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(Lights);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_lights->handle();

    vkUpdateDescriptorSets(m_vk_backend->device(), 1, &write_data, 0, nullptr);

    // Voxel grid uniform buffer
    DW_ZERO_MEMORY(buffer_info);
	DW_ZERO_MEMORY(write_data);

	buffer_info.buffer = m_ubo_voxel_grid->handle();
	buffer_info.offset = 0;
	buffer_info.range  = sizeof(VoxelizerData);

	write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write_data.descriptorCount = 1;
	write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	write_data.pBufferInfo     = &buffer_info;
	write_data.dstBinding      = 0;
	write_data.dstSet          = m_ds_voxel_grid_main->handle();

	vkUpdateDescriptorSets(m_vk_backend->device(), 1, &write_data, 0, nullptr);
}

void VCTRenderer::create_main_pipeline_state()
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

    pl_desc.add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(m_ds_layout_ubo)
        .add_descriptor_set_layout(m_shadow_map->m_ds_layout_sampler)
        .add_descriptor_set_layout(m_ds_layout_ubo)
        .add_descriptor_set_layout(m_voxelizer->m_ds_layout_voxel_grid_mip_maps)
        .add_descriptor_set_layout(m_ds_layout_voxel_grid_main);
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants));

    m_pipeline_layout_main = dw::vk::PipelineLayout::create(m_vk_backend, pl_desc);
    m_pipeline_layout_main->set_name("Main::pipeline_layout_main");

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
    m_graphics_pipeline_main->set_name("Main::graphics_pipeline_main");
}

bool VCTRenderer::load_object(std::string filename)
{
    m_meshes.push_back(dw::Mesh::load(m_vk_backend, filename));
    objects.push_back(RenderObject(m_meshes[m_meshes.size() - 1], m_vk_backend));
    return m_meshes[m_meshes.size() - 1] != nullptr;
}

enum Model
{
    SPONZA, DRAGON, STATUE
};

bool VCTRenderer::load_objects()
{
    std::vector<bool> results;
    Model             model = SPONZA;

    if (model == SPONZA)
    {
        results.push_back(load_object("models/sponza/Sponza.gltf"));
    }
    else if (model == DRAGON)
    {
        results.push_back(load_object("models/dragon.glb"));
        objects[objects.size() - 1].scale = 10.0f;
    }
    else if (model == STATUE)
    {
        results.push_back(load_object("models/statue.glb"));
        objects[objects.size() - 1].scale = 10.0f;
    }

    for (bool result : results) {
        if (!result)
            return false;
    }

    return true;
}

inline void VCTRenderer::create_camera()
{
    m_main_camera = std::make_unique<dw::Camera>(
        60.0f, 0.1f, m_far, float(m_width) / float(m_height), glm::vec3(0.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0, -1.0f));
}

void VCTRenderer::render_objects(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::PipelineLayout::Ptr pipeline_layout)
{
    VkDeviceSize offset = 0;

    for (auto object : objects)
    {
        m_mesh_push_constants.model = object.get_model();
        auto mesh = object.mesh;

        vkCmdBindVertexBuffers(cmd_buf->handle(), 0, 1, &mesh->vertex_buffer()->handle(), &offset);
        vkCmdBindIndexBuffer(cmd_buf->handle(), mesh->index_buffer()->handle(), 0, VK_INDEX_TYPE_UINT32);

        const auto& submeshes = mesh->sub_meshes();

        for (uint32_t i = 0; i < submeshes.size(); i++)
        {
            auto& submesh = submeshes[i];
            auto& mat     = mesh->material(submesh.mat_idx);

            vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout->handle(), 0, 1, &mat->descriptor_set()->handle(), 0, nullptr);
            vkCmdPushConstants(cmd_buf->handle(), pipeline_layout->handle(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(MeshPushConstants), &m_mesh_push_constants);

            // Issue draw call.
            vkCmdDrawIndexed(cmd_buf->handle(), submesh.index_count, 1, submesh.base_index, submesh.base_vertex, 0);
        }
    }
}

void VCTRenderer::begin_render_main(dw::vk::CommandBuffer::Ptr cmd_buf)
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

void VCTRenderer::revoxelize(int resolution)
{
    if (m_voxelizer->m_voxels_per_side != resolution)
    {
        m_voxelization_resolution = resolution;
        vkDeviceWaitIdle(m_vk_backend->device());
        m_voxelizer.reset();
        create_voxelizer();
        m_graphics_pipeline_main.reset();
        create_main_pipeline_state();
        
    }
}

void VCTRenderer::revoxelize(VoxelizationType type)
{
    if (m_voxelizer->m_voxelization_type != type)
    {
        m_voxelization_type = type;
        vkDeviceWaitIdle(m_vk_backend->device());
        m_voxelizer.reset();
        create_voxelizer();
        m_graphics_pipeline_main.reset();
        create_main_pipeline_state();
        
    }
}

void VCTRenderer::render(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    DW_SCOPED_SAMPLE("render", cmd_buf);

    m_shadow_map->begin_render(cmd_buf, m_vk_backend);
    {
        DW_SCOPED_SAMPLE("Shadow map", cmd_buf);
        render_objects(cmd_buf, m_shadow_map->m_pipeline_layout);
    }
    m_shadow_map->end_render(cmd_buf);

    //if (m_voxelizer->first_time)
    if (true)
    {
        m_voxelizer->transition_voxel_grid(cmd_buf);
        m_voxelizer->reset_voxel_grid(cmd_buf);
        //m_voxelizer->reset_voxelization_image_memory_barrier_voxel_grid(cmd_buf);
        m_voxelizer->debug_barrier(cmd_buf);

        m_voxelizer->begin_voxelization(cmd_buf, m_vk_backend);

        if (m_voxelizer->m_voxelization_type == GEOMETRY_SHADER_VOXELIZATION)
        {
            DW_SCOPED_SAMPLE("Geometry voxelizer", cmd_buf);
            GeometryVoxelizer* voxelization_ptr = dynamic_cast<GeometryVoxelizer*>(m_voxelizer.get());
            render_objects(cmd_buf, voxelization_ptr->m_pipeline_layout);
        }
        else if (m_voxelizer->m_voxelization_type == COMPUTE_SHADER_VOXELIZATION)
        {
            ComputeVoxelizer* voxelization_ptr = dynamic_cast<ComputeVoxelizer*>(m_voxelizer.get());
            voxelization_ptr->voxelize(cmd_buf, m_vk_backend, objects);
        }

        m_voxelizer->end_voxelization(cmd_buf);
        //m_voxelizer->voxelization_visualization_image_memory_barrier_voxel_grid(cmd_buf);
        m_voxelizer->debug_barrier(cmd_buf);


        m_voxelizer->reset_instance_buffer(cmd_buf);
        //m_voxelizer->reset_voxelization_buffer_memory_barrier_indirect(cmd_buf);
        m_voxelizer->debug_barrier(cmd_buf);

        //m_voxelizer->pre_mip_map_image_memory_barrier(cmd_buf);
        m_voxelizer->generate_mip_maps(cmd_buf);

        m_voxelizer->debug_barrier(cmd_buf);

        m_voxelizer->dispatch_visualization_compute_shader(m_vk_backend, cmd_buf);
        //m_voxelizer->visualization_main_buffer_memory_barrier(cmd_buf);
        m_voxelizer->debug_barrier(cmd_buf);

        m_voxelizer->first_time = false;
    }

    uint32_t lights_dynamic_offset = m_ubo_size_lights * m_vk_backend->current_frame_idx();
    uint32_t voxel_grid_dynamic_offset = m_ubo_size_voxel_grid * m_vk_backend->current_frame_idx();
    update_uniforms(cmd_buf);

    if (m_voxelization_visualization_enabled)
    {
        m_voxelizer->noTexture = m_mesh_push_constants.noTexture;
        m_voxelizer->begin_render_visualizer(cmd_buf, m_vk_backend);
        DW_SCOPED_SAMPLE("Visualization", cmd_buf);
        m_voxelizer->render_voxels(cmd_buf);
    }
    else
    {
        uint32_t dynamic_offset = m_ubo_size_main * m_vk_backend->current_frame_idx();
        begin_render_main(cmd_buf);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 1, 1, &m_ds_transforms_main->handle(), 1, &dynamic_offset);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 2, 1, &m_shadow_map->m_ds_shadow_sampler->handle(), 0, nullptr);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 3, 1, &m_ds_lights->handle(), 1, &lights_dynamic_offset);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 4, 1, &m_voxelizer->m_ds_voxel_grid_mip_maps->handle(), 0, nullptr);
        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout_main->handle(), 5, 1, &m_ds_voxel_grid_main->handle(), 1, &voxel_grid_dynamic_offset);
        DW_SCOPED_SAMPLE("Main render", cmd_buf);
        render_objects(cmd_buf, m_pipeline_layout_main);
    }


    render_gui(cmd_buf);
    vkCmdEndRenderPass(cmd_buf->handle());
}

void VCTRenderer::update_uniforms(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    DW_SCOPED_SAMPLE("update_uniforms", cmd_buf);

    m_transforms_main.view             = m_main_camera->m_view;
    m_transforms_main.projection       = m_main_camera->m_projection;
    m_transforms_main.lightSpaceMatrix = m_shadow_map->projection() * m_shadow_map->view();
    m_transforms_main.camera_pos       = glm::vec4(m_main_camera->m_position, 1.0f);
    uint8_t* ptr                       = (uint8_t*)m_ubo_transforms_main->mapped_ptr();
    memcpy(ptr + m_ubo_size_main * m_vk_backend->current_frame_idx(), &m_transforms_main, sizeof(TransformsMain));

    VoxelizerData voxelizer_data;
    AABB          aabb = m_voxelizer->get_AABB();
    voxelizer_data.AABB_min = glm::vec4(aabb.min, 1.0f);
    voxelizer_data.AABB_max = glm::vec4(aabb.max, 1.0f);
    ptr            = (uint8_t*)m_ubo_voxel_grid->mapped_ptr();
    memcpy(ptr + m_ubo_size_voxel_grid * m_vk_backend->current_frame_idx(), &voxelizer_data, sizeof(VoxelizerData));

    /*m_transforms_main.view             = m_voxelizer->get_view();
    m_transforms_main.projection       = m_voxelizer->get_proj();
    m_transforms_main.lightSpaceMatrix = m_shadow_map->projection() * m_shadow_map->view();
    uint8_t* ptr                       = (uint8_t*)m_ubo_transforms_main->mapped_ptr();
    memcpy(ptr + m_ubo_size_main * m_vk_backend->current_frame_idx(), &m_transforms_main, sizeof(TransformsMain));*/

    m_voxelizer->m_visualizer_transforms.view = m_transforms_main.view;
    m_voxelizer->m_visualizer_transforms.projection = m_transforms_main.projection;
    m_voxelizer->m_visualizer_transforms.model      = m_voxelizer->m_cube.get_model();
    ptr = (uint8_t*)m_voxelizer->m_visualizer_ubo_data->mapped_ptr();
    memcpy(ptr + m_voxelizer->m_visualizer_ubo_size * m_vk_backend->current_frame_idx(), &m_voxelizer->m_visualizer_transforms, sizeof(VisualizerUBO));

    ptr = (uint8_t*)m_ubo_lights->mapped_ptr();
    memcpy(ptr + m_ubo_size_lights * m_vk_backend->current_frame_idx(), &m_lights, sizeof(Lights));
}

void VCTRenderer::update_camera()
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

DW_DECLARE_MAIN(VCTRenderer)
