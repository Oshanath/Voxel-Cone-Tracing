#include "RendererObject.h"

dw::vk::DescriptorSetLayout::Ptr m_ds_layout_vertex_index;

void RenderObject::initialize_common_resources(dw::vk::Backend::Ptr backend)
{
    dw::vk::DescriptorSetLayout::Desc desc;
    DW_ZERO_MEMORY(desc);
    desc.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    desc.add_binding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
    m_ds_layout_vertex_index = dw::vk::DescriptorSetLayout::create(backend, desc);
    m_ds_layout_vertex_index->set_name("Voxelizer::m_ds_layout_vertex_index");
}

dw::vk::DescriptorSetLayout::Ptr RenderObject::get_ds_layout_vertex_index()
{
    return m_ds_layout_vertex_index;
}

void RenderObject::reset_m_ds_layout_vertex_index()
{
    m_ds_layout_vertex_index.reset();
}