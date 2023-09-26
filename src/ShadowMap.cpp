#include "ShadowMap.h"
#include <gtc/matrix_transform.hpp>
#include <macros.h>
#include <imgui.h>
#include <vk_mem_alloc.h>

ShadowMap::ShadowMap(dw::vk::Backend::Ptr backend, uint32_t m_size, const dw::vk::VertexInputStateDesc& vertex_input_state) :
    m_size(m_size)
{
    m_image      = dw::vk::Image::create(backend, VK_IMAGE_TYPE_2D, m_size, m_size, 1, 1, 1, VK_FORMAT_D32_SFLOAT, VMA_MEMORY_USAGE_GPU_ONLY, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_LAYOUT_UNDEFINED);
    m_image_view = dw::vk::ImageView::create(backend, m_image, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1);

    VkAttachmentDescription attachment;
    DW_ZERO_MEMORY(attachment);

    // Depth attachment
    attachment.format         = VK_FORMAT_D32_SFLOAT;
    attachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    attachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_reference;
    depth_reference.attachment = 0;
    depth_reference.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    std::vector<VkSubpassDescription> subpass_description(1);

    subpass_description[0].pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_description[0].colorAttachmentCount    = 0;
    subpass_description[0].pColorAttachments       = nullptr;
    subpass_description[0].pDepthStencilAttachment = &depth_reference;
    subpass_description[0].inputAttachmentCount    = 0;
    subpass_description[0].pInputAttachments       = nullptr;
    subpass_description[0].preserveAttachmentCount = 0;
    subpass_description[0].pPreserveAttachments    = nullptr;
    subpass_description[0].pResolveAttachments     = nullptr;

    // Subpass dependencies for layout transitions
    std::vector<VkSubpassDependency> dependencies(2);

    dependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass      = 0;
    dependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass      = 0;
    dependencies[1].dstSubpass      = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[1].srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    m_render_pass = dw::vk::RenderPass::create(backend, { attachment }, subpass_description, dependencies);
    m_framebuffer = dw::vk::Framebuffer::create(backend, m_render_pass, { m_image_view }, m_size, m_size, 1);

    // Shadow map sampler
    dw::vk::Sampler::Desc sampler_desc;
    DW_ZERO_MEMORY(sampler_desc);
    sampler_desc.mag_filter     = VK_FILTER_LINEAR;
    sampler_desc.min_filter     = VK_FILTER_LINEAR;
    sampler_desc.mipmap_mode    = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_desc.address_mode_u = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_v = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.address_mode_w = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_desc.mip_lod_bias   = 0.0f;
    sampler_desc.max_anisotropy = 1.0f;
    sampler_desc.min_lod        = 0.0f;
    sampler_desc.max_lod        = 1.0f;
    sampler_desc.compare_enable = VK_FALSE;
    sampler_desc.compare_op     = VK_COMPARE_OP_NEVER;
    m_shadow_map_sampler        = dw::vk::Sampler::create(backend, sampler_desc);

    create_descriptor_sets(backend);
    create_pipeline_state(backend, vertex_input_state);
    update();
}

void ShadowMap::create_descriptor_sets(dw::vk::Backend::Ptr backend)
{

    m_ubo_size = backend->aligned_dynamic_ubo_size(sizeof(TransformsShadow));
    m_ubo_transforms = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, m_ubo_size * dw::vk::Backend::kMaxFramesInFlight, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);

    dw::vk::DescriptorSetLayout::Desc desc;

    // UBO layout
    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    m_ds_layout_ubo = dw::vk::DescriptorSetLayout::create(backend, desc);

    // Shadow map sampler layout
    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
    m_ds_layout_sampler = dw::vk::DescriptorSetLayout::create(backend, desc);

    m_ds_transforms = backend->allocate_descriptor_set(m_ds_layout_ubo);
    m_ds_shadow_sampler = backend->allocate_descriptor_set(m_ds_layout_sampler);

    // --------------------------------------------------------------------------

    VkDescriptorBufferInfo buffer_info;
    VkWriteDescriptorSet   write_data;
    VkDescriptorImageInfo  image_info;

    // UBO
    DW_ZERO_MEMORY(buffer_info);
    DW_ZERO_MEMORY(write_data);

    buffer_info.buffer = m_ubo_transforms->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(TransformsShadow);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    write_data.pBufferInfo     = &buffer_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_transforms->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);


    // Shadow sampler
    DW_ZERO_MEMORY(image_info);
    DW_ZERO_MEMORY(write_data);

    image_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    image_info.imageView   = m_image_view->handle();
    image_info.sampler     = m_shadow_map_sampler->handle();

    DW_ZERO_MEMORY(write_data);

    write_data.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data.descriptorCount = 1;
    write_data.descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data.pImageInfo      = &image_info;
    write_data.dstBinding      = 0;
    write_data.dstSet          = m_ds_shadow_sampler->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

}

void ShadowMap::create_pipeline_state(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state)
{
    // ---------------------------------------------------------------------------
    // Create shader modules
    // ---------------------------------------------------------------------------

    dw::vk::ShaderModule::Ptr vs = dw::vk::ShaderModule::create_from_file(backend, "shaders/shadow.vert.spv");
    dw::vk::ShaderModule::Ptr fs = dw::vk::ShaderModule::create_from_file(backend, "shaders/shadow.frag.spv");

    dw::vk::GraphicsPipeline::Desc pso_desc;

    pso_desc.add_shader_stage(VK_SHADER_STAGE_VERTEX_BIT, vs, "main")
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

    vp_desc.add_viewport(0.0f, 0.0f, m_size, m_size, 0.0f, 1.0f)
        .add_scissor(0, 0, m_size, m_size);

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

    pl_desc.add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(m_ds_layout_ubo)
        .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants));

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

ShadowMap::~ShadowMap()
{
    m_framebuffer.reset();
    m_render_pass.reset();
    m_image_view.reset();
    m_image.reset();
    m_shadow_map_sampler.reset();
    m_ubo_transforms.reset();
    m_ds_transforms.reset();
    m_ds_shadow_sampler.reset();
    m_pipeline.reset();
    m_ds_layout_ubo.reset();
    m_ds_layout_sampler.reset();
    m_pipeline_layout.reset();
}

void ShadowMap::begin_render(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{
    VkClearValue clear_value;

    clear_value.depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo info    = {};
    info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass               = m_render_pass->handle();
    info.framebuffer              = m_framebuffer->handle();
    info.renderArea.extent.width  = m_size;
    info.renderArea.extent.height = m_size;
    info.clearValueCount          = 1;
    info.pClearValues             = &clear_value;

    vkCmdBeginRenderPass(cmd_buf->handle(), &info, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp;

    vp.x        = 0.0f;
    vp.y        = 0.0f;
    vp.width    = (float)m_size;
    vp.height   = (float)m_size;
    vp.minDepth = 0.0f;
    vp.maxDepth = 1.0f;

    vkCmdSetViewport(cmd_buf->handle(), 0, 1, &vp);

    VkRect2D scissor_rect;

    scissor_rect.extent.width  = m_size;
    scissor_rect.extent.height = m_size;
    scissor_rect.offset.x      = 0;
    scissor_rect.offset.y      = 0;

    vkCmdSetScissor(cmd_buf->handle(), 0, 1, &scissor_rect);

    // Set depth bias (aka "Polygon offset")
    // Required to avoid shadow mapping artifacts
    vkCmdSetDepthBias(cmd_buf->handle(), depthBiasConstant, 0.0f, depthBiasSlope);

    // update buffers
    m_transforms.view       = m_view;
    m_transforms.projection = m_projection;
    uint8_t* ptr                   = (uint8_t*)m_ubo_transforms->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_transforms, sizeof(TransformsShadow));

    uint32_t dynamic_offset = m_ubo_size * backend->current_frame_idx();
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout->handle(), 1, 1, &m_ds_transforms->handle(), 1, &dynamic_offset);
}

void ShadowMap::end_render(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    vkCmdEndRenderPass(cmd_buf->handle());
}

void ShadowMap::gui()
{
    ImGui::SliderFloat("Extents", &m_extents, 1.0f, 10000.0f);
    ImGui::SliderFloat("Near Plane", &m_near_plane, 1.0f, 10000.0f);
    ImGui::SliderFloat("Far Plane", &m_far_plane, 1.0f, 10000.0f);
    ImGui::SliderFloat("Back Off Distance", &m_backoff_distance, 1.0f, 10000.0f);
}

void ShadowMap::set_direction(const glm::vec3& d)
{
    m_light_direction = d;
    update();
}

void ShadowMap::set_target(const glm::vec3& t)
{
    m_light_target = t;
    update();
}

void ShadowMap::set_color(const glm::vec3& c)
{
    m_light_color = c;
    update();
}

void ShadowMap::set_near_plane(const float& n)
{
    m_near_plane = n;
    update();
}

void ShadowMap::set_far_plane(const float& f)
{
    m_far_plane = f;
    update();
}

void ShadowMap::set_extents(const float& e)
{
    m_extents = e;
    update();
}

void ShadowMap::set_backoff_distance(const float& d)
{
    m_backoff_distance = d;
    update();
}

void ShadowMap::update()
{
    glm::vec3 light_camera_pos = m_light_target - m_light_direction * m_backoff_distance;
    m_view                     = glm::lookAt(light_camera_pos, m_light_target, glm::vec3(0.0f, 1.0f, 0.0f));
    m_projection               = glm::ortho(-m_extents, m_extents, -m_extents, m_extents, m_near_plane, m_far_plane);
}