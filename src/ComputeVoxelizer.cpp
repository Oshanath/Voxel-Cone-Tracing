#include "ComputeVoxelizer.h"
#include <iostream>
#include <profiler.h>

ComputeVoxelizer::ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, uint32_t m_viewport_width, uint32_t m_viewport_height, std::vector<RenderObject>& objects) :
    Voxelizer(backend, AABB_min, AABB_max, voxels_per_side, vertex_input_state, COMPUTE_SHADER_VOXELIZATION, m_viewport_width, m_viewport_height)
{
    create_descriptor_sets(backend, objects);
    create_indirect_reset_pipeline_state(backend);
    create_voxelizer_pipeline_state(backend);
    this->m_compute_voxelization_type = CORRECT_TEXCOORDS;
    m_push_constants.large_triangel_threshold = 15;
}

void ComputeVoxelizer::create_voxelizer_pipeline_state(dw::vk::Backend::Ptr backend)
{
    // correct texcoords
    dw::vk::ShaderModule::Ptr     cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/compute_voxelizer_correct_texcoords.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_image)
        .add_descriptor_set_layout(m_ds_layout_ubo_dynamic)
        .add_descriptor_set_layout(m_ds_layout_ubo_dynamic)
        .add_descriptor_set_layout(RenderObject::get_ds_layout_vertex_index())
        .add_descriptor_set_layout(m_ds_layout_bindless)
        .add_descriptor_set_layout(m_ds_layout_bindless_buffer)
        .add_descriptor_set_layout(m_ds_layout_indirect_compute_buffer)
        .add_descriptor_set_layout(m_ds_layout_large_triangle_buffer);

    pl_desc.add_push_constant_range(VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeVoxelizerPushConstants));
    m_pipeline_layout = dw::vk::PipelineLayout::create(backend, pl_desc);
    m_pipeline_layout->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline_layout");

    pso_desc.set_pipeline_layout(m_pipeline_layout);
    m_pipeline_correct_texcoords = dw::vk::ComputePipeline::create(backend, pso_desc);
    m_pipeline_correct_texcoords->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline_correct_texcoords");

    // incorrect texcoords
    dw::vk::ShaderModule::Ptr     cs2 = dw::vk::ShaderModule::create_from_file(backend, "shaders/compute_voxelizer_incorrect_texcoords.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc2;
    pso_desc2.set_shader_stage(cs2, "main");

    pso_desc2.set_pipeline_layout(m_pipeline_layout);
    m_pipeline_incorrect_texcoords = dw::vk::ComputePipeline::create(backend, pso_desc2);
    m_pipeline_incorrect_texcoords->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline_incorrect_texcoords");

    // large triangle
    dw::vk::ShaderModule::Ptr     cs3 = dw::vk::ShaderModule::create_from_file(backend, "shaders/large_triangles_dda.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc3;
    pso_desc3.set_shader_stage(cs3, "main");

    pso_desc3.set_pipeline_layout(m_pipeline_layout);
    m_pipeline_large_triangle = dw::vk::ComputePipeline::create(backend, pso_desc3);
    m_pipeline_large_triangle->set_name("Voxelizer::m_compute_voxelizer_compute_pipeline_large_triangle");

}

void ComputeVoxelizer::create_descriptor_sets(dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects)
{
    // Large triangle buffer
    m_large_triangle_buffer_size = backend->aligned_dynamic_ubo_size(sizeof(LargeTriangle) * 200000);
    m_large_triangle_buffer      = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_large_triangle_buffer_size, VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_large_triangle_buffer->set_name("ComputeVoxelizer::m_large_triangle_buffer");

    dw::vk::DescriptorSetLayout::Desc desc_large_triangle_buffer;
    desc_large_triangle_buffer.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_large_triangle_buffer = dw::vk::DescriptorSetLayout::create(backend, desc_large_triangle_buffer);
    m_ds_layout_large_triangle_buffer->set_name("ComputeVoxelizer::m_ds_layout_large_triangle_buffer");

    m_ds_large_triangle_buffer = backend->allocate_descriptor_set(m_ds_layout_large_triangle_buffer);
    m_ds_large_triangle_buffer->set_name("ComputeVoxelizer::m_ds_large_triangle_buffer");

    VkDescriptorBufferInfo buffer_info_large_triangle;
    buffer_info_large_triangle.buffer = m_large_triangle_buffer->handle();
    buffer_info_large_triangle.offset = 0;
    buffer_info_large_triangle.range  = m_large_triangle_buffer_size;

    VkWriteDescriptorSet write_data_large_triangle;
    DW_ZERO_MEMORY(write_data_large_triangle);
    write_data_large_triangle.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data_large_triangle.descriptorCount = 1;
    write_data_large_triangle.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_data_large_triangle.pBufferInfo     = &buffer_info_large_triangle;
    write_data_large_triangle.dstBinding      = 0;
    write_data_large_triangle.dstSet          = m_ds_large_triangle_buffer->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data_large_triangle, 0, nullptr);

    // indirect compute buffer
    m_indirect_compute_buffer_size = backend->aligned_dynamic_ubo_size(sizeof(VkDispatchIndirectCommand));
    m_indirect_compute_buffer      = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, m_indirect_compute_buffer_size, VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_indirect_compute_buffer->set_name("ComputeVoxelizer::m_indirect_compute_buffer");

    VkDispatchIndirectCommand indirect_command;
    indirect_command.x = 0;
    indirect_command.y = 1;
    indirect_command.z = 1;

    uint8_t* ptr = (uint8_t*)m_indirect_compute_buffer->mapped_ptr();
    memcpy(ptr, &indirect_command, sizeof(VkDispatchIndirectCommand));

    dw::vk::DescriptorSetLayout::Desc desc_indirect_compute_buffer;
    desc_indirect_compute_buffer.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_indirect_compute_buffer = dw::vk::DescriptorSetLayout::create(backend, desc_indirect_compute_buffer);
    m_ds_layout_indirect_compute_buffer->set_name("ComputeVoxelizer::m_ds_layout_indirect_compute_buffer");

    m_ds_indirect_compute_buffer = backend->allocate_descriptor_set(m_ds_layout_indirect_compute_buffer);
    m_ds_indirect_compute_buffer->set_name("ComputeVoxelizer::m_ds_indirect_compute_buffer");

    VkDescriptorBufferInfo buffer_info_indirect_compute;
    buffer_info_indirect_compute.buffer = m_indirect_compute_buffer->handle();
    buffer_info_indirect_compute.offset = 0;
    buffer_info_indirect_compute.range  = m_indirect_compute_buffer_size;

    VkWriteDescriptorSet write_data_indirect_compute;
    DW_ZERO_MEMORY(write_data_indirect_compute);
    write_data_indirect_compute.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data_indirect_compute.descriptorCount = 1;
    write_data_indirect_compute.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	write_data_indirect_compute.pBufferInfo     = &buffer_info_indirect_compute;
    write_data_indirect_compute.dstBinding      = 0;
    write_data_indirect_compute.dstSet          = m_ds_indirect_compute_buffer->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data_indirect_compute, 0, nullptr);

    // submesh count
    uint32_t submesh_count = 0;
    for (auto object : objects)
    {
		auto mesh = object.mesh;
		submesh_count += mesh->sub_meshes().size();
	}

    // bindless
    dw::vk::DescriptorSetLayout::Desc desc;
    for (int i = 0; i < 5; i++)
    {
        desc.add_binding(i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, submesh_count, VK_SHADER_STAGE_ALL);
    }
    m_ds_layout_bindless = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_bindless->set_name("ComputeVoxelizer::ds_layout_bindless");

    m_ds_bindless = backend->allocate_descriptor_set(m_ds_layout_bindless);
    m_ds_bindless->set_name("ComputeVoxelizer::ds_bindless");

    std::vector<VkDescriptorImageInfo> image_infos[5];
    for (int i = 0; i < 5; i++) { image_infos[i] = std::vector<VkDescriptorImageInfo>(); }

    std::vector<uint32_t> triangle_submesh_map;

    for (auto object : objects)
    {
        auto        mesh      = object.mesh;
        const auto& submeshes = mesh->sub_meshes();

        for (uint32_t i = 0; i < submeshes.size(); i++)
        {
            auto& submesh = submeshes[i];
            auto& mat     = mesh->material(submesh.mat_idx);

            VkDescriptorImageInfo image_info[5];

            image_info[0].sampler = dw::Material::common_sampler()->handle();
            // image_info[0].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[0].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[0].push_back(image_info[0]);

            image_info[1].sampler     = dw::Material::common_sampler()->handle();
            image_info[1].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[1].push_back(image_info[1]);

            image_info[2].sampler     = dw::Material::common_sampler()->handle();
            image_info[2].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[2].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[2].push_back(image_info[2]);

            image_info[3].sampler     = dw::Material::common_sampler()->handle();
            image_info[3].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[3].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[3].push_back(image_info[3]);

            image_info[4].sampler     = dw::Material::common_sampler()->handle();
            image_info[4].imageView   = mat->m_albedo_idx != -1 ? mat->m_image_views[mat->m_albedo_idx]->handle() : mat->m_default_image_view->handle();
            image_info[4].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            image_infos[4].push_back(image_info[4]);

            // add triangles to map
            uint32_t triangle_count = submesh.index_count / 3;
            for (int j = 0; j < triangle_count; j++) {
                triangle_submesh_map.push_back(i);
            }
        }
    }

    VkWriteDescriptorSet write_data[5];
    for (int i = 0; i < 5; i++) { DW_ZERO_MEMORY(write_data[i]); }

    write_data[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data[0].descriptorCount = image_infos[0].size();
    write_data[0].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data[0].pImageInfo      = image_infos[0].data();
    write_data[0].dstBinding      = 0;
    write_data[0].dstSet          = m_ds_bindless->handle();

    write_data[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data[1].descriptorCount = image_infos[1].size();
    write_data[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data[1].pImageInfo      = image_infos[1].data();
    write_data[1].dstBinding      = 1;
    write_data[1].dstSet          = m_ds_bindless->handle();

    write_data[2].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data[2].descriptorCount = image_infos[2].size();
    write_data[2].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data[2].pImageInfo      = image_infos[2].data();
    write_data[2].dstBinding      = 2;
    write_data[2].dstSet          = m_ds_bindless->handle();

    write_data[3].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data[3].descriptorCount = image_infos[3].size();
    write_data[3].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data[3].pImageInfo      = image_infos[3].data();
    write_data[3].dstBinding      = 3;
    write_data[3].dstSet          = m_ds_bindless->handle();

    write_data[4].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data[4].descriptorCount = image_infos[4].size();
    write_data[4].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_data[4].pImageInfo      = image_infos[4].data();
    write_data[4].dstBinding      = 4;
    write_data[4].dstSet          = m_ds_bindless->handle();

    vkUpdateDescriptorSets(backend->device(), 5, write_data, 0, nullptr);

    // bindless buffer
    m_bindless_buffer = dw::vk::Buffer::create(backend, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, sizeof(uint32_t) * triangle_submesh_map.size(), VMA_MEMORY_USAGE_CPU_TO_GPU, VMA_ALLOCATION_CREATE_MAPPED_BIT);
    m_bindless_buffer->set_name("ComputeVoxelizer::m_bindless_buffer");
    memcpy(m_bindless_buffer->mapped_ptr(), triangle_submesh_map.data(), sizeof(uint32_t) * triangle_submesh_map.size());

    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_bindless_buffer = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_bindless_buffer->set_name("ComputeVoxelizer::m_ds_layout_bindless_buffer");

    m_ds_bindless_buffer = backend->allocate_descriptor_set(m_ds_layout_bindless_buffer);
    m_ds_bindless_buffer->set_name("ComputeVoxelizer::m_ds_bindless_buffer");

    VkDescriptorBufferInfo buffer_info;
    buffer_info.buffer = m_bindless_buffer->handle();
    buffer_info.offset = 0;
    buffer_info.range  = sizeof(uint32_t) * triangle_submesh_map.size();

    VkWriteDescriptorSet write_data2;
    DW_ZERO_MEMORY(write_data2);
    write_data2.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_data2.descriptorCount = 1;
    write_data2.descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    write_data2.pBufferInfo     = &buffer_info;
    write_data2.dstBinding      = 0;
    write_data2.dstSet          = m_ds_bindless_buffer->handle();

    vkUpdateDescriptorSets(backend->device(), 1, &write_data2, 0, nullptr);
}

void ComputeVoxelizer::begin_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{
    reset_indirect_buffer(cmd_buf);
    reset_compute_indirect_buffer_memory_barrier(cmd_buf);

    AABB aabb       = get_AABB();
    m_data.AABB_min = glm::vec4(aabb.min, 1.0f);
    m_data.AABB_max = glm::vec4(aabb.max, 1.0f);

    uint8_t* ptr = (uint8_t*)m_ubo_data->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_data, sizeof(VoxelizerData));

    uint32_t offset = 0;

	vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_correct_texcoords->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 0, 1, &m_ds_image->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 1, 1, &m_ds_data->handle(), 1, &offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 2, 1, &m_ds_view_proj_ubo->handle(), 1, &offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 4, 1, &m_ds_bindless->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 5, 1, &m_ds_bindless_buffer->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 6, 1, &m_ds_indirect_compute_buffer->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 7, 1, &m_ds_large_triangle_buffer->handle(), 0, 0);
}

void ComputeVoxelizer::begin_large_triangle_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend)
{
    large_triangle_buffer_memory_barrier(cmd_buf);

    AABB aabb       = get_AABB();
    m_data.AABB_min = glm::vec4(aabb.min, 1.0f);
    m_data.AABB_max = glm::vec4(aabb.max, 1.0f);

    uint8_t* ptr = (uint8_t*)m_ubo_data->mapped_ptr();
    memcpy(ptr + m_ubo_size * backend->current_frame_idx(), &m_data, sizeof(VoxelizerData));

    uint32_t offset = 0;

    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_large_triangle->handle());

    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 0, 1, &m_ds_image->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 1, 1, &m_ds_data->handle(), 1, &offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 2, 1, &m_ds_view_proj_ubo->handle(), 1, &offset);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 4, 1, &m_ds_bindless->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 5, 1, &m_ds_bindless_buffer->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 6, 1, &m_ds_indirect_compute_buffer->handle(), 0, 0);
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 7, 1, &m_ds_large_triangle_buffer->handle(), 0, 0);
}

void ComputeVoxelizer::create_indirect_reset_pipeline_state(dw::vk::Backend::Ptr backend)
{
    dw::vk::ShaderModule::Ptr     cs = dw::vk::ShaderModule::create_from_file(backend, "shaders/reset_compute_indirect.comp.spv");
    dw::vk::ComputePipeline::Desc pso_desc;
    pso_desc.set_shader_stage(cs, "main");

    dw::vk::PipelineLayout::Desc pl_desc;
    pl_desc.add_descriptor_set_layout(m_ds_layout_indirect_compute_buffer);
    m_pipeline_layout_indirect_reset = dw::vk::PipelineLayout::create(backend, pl_desc);

    pso_desc.set_pipeline_layout(m_pipeline_layout_indirect_reset);
    m_pipeline_indirect_reset = dw::vk::ComputePipeline::create(backend, pso_desc);
}

void ComputeVoxelizer::reset_indirect_buffer(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    vkCmdBindPipeline(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_indirect_reset->handle());
    vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout_indirect_reset->handle(), 0, 1, &m_ds_indirect_compute_buffer->handle(), 0, nullptr);
    vkCmdDispatch(cmd_buf->handle(), 1, 1, 1);
}

void ComputeVoxelizer::reset_compute_indirect_buffer_memory_barrier(dw::vk::CommandBuffer::Ptr cmd_buf)
{
    VkBufferMemoryBarrier barrier = {};
    barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    barrier.buffer                = m_indirect_compute_buffer->handle();
    barrier.srcAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.size                  = m_indirect_compute_buffer_size;
    barrier.offset                = 0;
    barrier.pNext                 = nullptr;

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &barrier, 0, nullptr);
}

void ComputeVoxelizer::large_triangle_buffer_memory_barrier(dw::vk::CommandBuffer::Ptr cmd_buf)
{
	VkBufferMemoryBarrier barrier = {};
	barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	barrier.buffer                = m_large_triangle_buffer->handle();
	barrier.srcAccessMask         = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask         = VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
	barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
	barrier.size                  = m_large_triangle_buffer_size;
	barrier.offset                = 0;
	barrier.pNext                 = nullptr;

	vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 1, &barrier, 0, nullptr);
}

void ComputeVoxelizer::voxelize(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::Backend::Ptr backend, std::vector<RenderObject>& objects)
{
    DW_SCOPED_SAMPLE("Compute Voxelizer", cmd_buf);

    {
        DW_SCOPED_SAMPLE("Small Triangles", cmd_buf);
        VkDeviceSize offset = 0;

        for (auto object : objects)
        {
            auto mesh = object.mesh;

            vkCmdBindDescriptorSets(cmd_buf->handle(), VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline_layout->handle(), 3, 1, &object.m_ds_vertex_index->handle(), 0, 0);

            int local_size      = 32;
            int triangle_count  = mesh->indices().size() / 3;
            int workgroup_count = ceil(double(triangle_count) / double(local_size));

            m_push_constants.model          = object.get_model();
            m_push_constants.triangle_count = triangle_count;

            vkCmdPushConstants(cmd_buf->handle(), m_pipeline_layout->handle(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeVoxelizerPushConstants), &m_push_constants);
            vkCmdDispatch(cmd_buf->handle(), workgroup_count, 1, 1);
        }    
    
    }
    debug_barrier(cmd_buf);
    {
        DW_SCOPED_SAMPLE("Large Triangles", cmd_buf);
        begin_large_triangle_voxelization(cmd_buf, backend);
        vkCmdDispatchIndirect(cmd_buf->handle(), m_indirect_compute_buffer->handle(), 0);
    }

    vkCmdPipelineBarrier(cmd_buf->handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, nullptr, 0, nullptr, 0, nullptr);
}

void ComputeVoxelizer::end_voxelization(dw::vk::CommandBuffer::Ptr cmd_buf)
{

}
