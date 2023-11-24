#include "ComputeVoxelizer.h"

ComputeVoxelizer::ComputeVoxelizer(dw::vk::Backend::Ptr backend, glm::vec3 AABB_min, glm::vec3 AABB_max, uint32_t voxels_per_side, const dw::vk::VertexInputStateDesc& vertex_input_state, VoxelizationType voxelization_type, uint32_t m_viewport_width, uint32_t m_viewport_height) :
    Voxelizer(backend, AABB_min, AABB_max, voxels_per_side, vertex_input_state, COMPUTE_SHADER_VOXELIZATION, m_viewport_width, m_viewport_height)
{
    
}

void ComputeVoxelizer::create_descriptor_sets(dw::vk::Backend::Ptr backend)
{
    dw::vk::DescriptorSetLayout::Desc desc;

    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_image = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_image->set_name("ComputeVoxelizer::m_ds_layout_image");

    m_ds_image = backend->allocate_descriptor_set(m_ds_layout_image);

    // --------------------------------------------------------------------------------------

    VkWriteDescriptorSet  write_data;
    VkDescriptorImageInfo image_info;

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

void ComputeVoxelizer::create_pipelines(dw::vk::Backend::Ptr backend, const dw::vk::VertexInputStateDesc& vertex_input_state)
{

}
