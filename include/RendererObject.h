#pragma once

#include <mesh.h>
#include <glm.hpp>
#include <gtc/quaternion.hpp>
#include <gtc/matrix_transform.hpp>
#include "util.h"

class RenderObject
{

public:
    glm::vec3 position;
    glm::quat rotation;
    float scale;
    dw::Mesh::Ptr mesh;
    dw::vk::DescriptorSet::Ptr m_ds_vertex_index;

    static void initialize_common_resources(dw::vk::Backend::Ptr backend);
    static dw::vk::DescriptorSetLayout::Ptr get_ds_layout_vertex_index();
    static void reset_m_ds_layout_vertex_index();

    inline RenderObject(dw::Mesh::Ptr mesh, dw::vk::Backend::Ptr backend) :
        position(glm::vec3(0.0f, 0.0f, 0.0f)),
        rotation(glm::angleAxis(0.0f, glm::vec3(0.0f, 1.0f, 0.0f))),
        scale(1.0f),
        mesh(mesh) {

        m_ds_vertex_index = backend->allocate_descriptor_set(get_ds_layout_vertex_index());

        VkWriteDescriptorSet  write_data;
        VkDescriptorBufferInfo buffer_info;

        DW_ZERO_MEMORY(write_data);
        DW_ZERO_MEMORY(buffer_info);

        buffer_info.buffer = mesh->vertex_buffer()->handle();
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;

        write_data.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_data.descriptorCount = 1;
        write_data.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_data.pBufferInfo = &buffer_info;
        write_data.dstBinding = 0;
        write_data.dstSet = m_ds_vertex_index->handle();

        vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

        DW_ZERO_MEMORY(write_data);
        DW_ZERO_MEMORY(buffer_info);

        buffer_info.buffer = mesh->index_buffer()->handle();
        buffer_info.offset = 0;
        buffer_info.range = VK_WHOLE_SIZE;

        write_data.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_data.descriptorCount = 1;
        write_data.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write_data.pBufferInfo = &buffer_info;
        write_data.dstBinding = 1;
        write_data.dstSet = m_ds_vertex_index->handle();

        vkUpdateDescriptorSets(backend->device(), 1, &write_data, 0, nullptr);

    }

    inline glm::mat4 get_model()
    {
        auto model = glm::mat4(1.0f);
        model = glm::scale(model, glm::vec3(scale));
        model = glm::mat4_cast(rotation) * model;
        model = glm::translate(model, position);
        return model;
    }

    inline void reset()
    {
        mesh.reset();
        m_ds_vertex_index.reset();
        reset_m_ds_layout_vertex_index();
    }
};