#include "ComputeVoxelizer.h"

ComputeVoxelizer::ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height) :
    Voxelizer(backend, AABB_min, AABB_max, voxels_per_side, vertex_input_state, COMPUTE_SHADER_VOXELIZATION, m_viewport_width, m_viewport_height)
{
    create_descriptor_sets(backend);
    create_voxelizer_pipeline_state(backend);
}

void ComputeVoxelizer::create_voxelizer_pipeline_state(dw::vk::Backend::Ptr backend)
{
    dw::vk::ShaderModule::Ptr     cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/compute_voxelizer.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_image)
        .add_descriptor_set_layout(m_ds_layout_ubo_static)
        .add_descriptor_set_layout(m_ds_layout_ubo_dynamic)
        .add_descriptor_set_layout(dw::Material::descriptor_set_layout())
        .add_descriptor_set_layout(RenderObject::get_ds_layout_vertex_index());
    pl_desc.add_push_constant_range(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshPushConstants));
    m_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);
    m_pipeline_layout->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline_layout");

    pso_desc.set_pipeline_layout(m_pipeline_layout);
    m_pipeline = dw::vk::ComputePipeline::create(backend, pso_desc);
    m_pipeline->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline");
}

void ComputeVoxelizer::create_descriptor_sets(dw::vk::Backend::Ptr backend)
{
    
}

void ComputeVoxelizer::begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{
    AABB aabb       = get_AABB();
    m_data.AABB_min = glm::vec4(aabb.min, 1.0f);
    m_data.AABB_max = glm::vec4(aabb.max, 1.0f);

    uint8_t* ptr = (uint8_t*)m_ubo_data->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_data, sizeof(VoxelizerData));

    uint32_t offset = 0;

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 0, 1, &m_ds_image->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 1, 1, &m_ds_voxel_grid_ubo->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 2, 1, &m_ds_view_proj_ubo->handle(), 1, &offset);
}

void ComputeVoxelizer::voxelize(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects)
{
    VkDeviceSize offset = 0;

    for (auto object : objects)
    {
        auto mesh = object.mesh;

        vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 4, 1, &object.m_ds_vertex_index->handle(), 0, 0);
        vkCmdPushConstants(cmd_buf->handle(), m_pipeline_layout->handle(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(MeshPushConstants), glm::value_ptr(object.get_model()));

        const auto& submeshes = mesh->sub_meshes();

        for (uint32_t i = 0; i < submeshes.size(); i++)
        {
            auto& submesh = submeshes[i];
            auto& mat     = mesh->material(submesh.mat_idx);

            vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 3, 1, &mat->descriptor_set()->handle(), 0, nullptr);

            // Issue draw call.
        }

        vkCmdDispatch(cmd_buf->handle(), mesh->indices().size() / 3, 1, 1);

    }
}

void ComputeVoxelizer::end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf)
{

}
