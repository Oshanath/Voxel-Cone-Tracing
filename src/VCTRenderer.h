#pragma once

#include <application.h>
#include <camera.h>
#include <material.h>
#include <mesh.h>
#include <vk.h>
#include <profiler.h>
#include <assimp/scene.h>
#include <vk_mem_alloc.h>
#include <iostream>
#include "RendererObject.h"
#include <shadow_map.h>

// Uniform buffer data structures.
struct TransformsMain
{
    DW_ALIGNED(16)
    glm::mat4 view;
    DW_ALIGNED(16)
    glm::mat4 projection;
    DW_ALIGNED(16)
    glm::mat4 lightSpaceMatrix;
};

struct TransformsShadow
{
    DW_ALIGNED(16)
    glm::mat4 view;
    DW_ALIGNED(16)
    glm::mat4 projection;
};

// Push constant data structure (model matrix)
struct MeshPushConstants
{
    DW_ALIGNED(16)
    glm::mat4 model;
};

class Sample : public dw::Application
{
protected:
    bool init(int argc, const char* argv[]) override;
    void update(double delta) override;
    void shutdown() override;
    dw::AppSettings intial_app_settings() override;
    inline void window_resized(int width, int height) override
    {
        // Override window resized method to update camera projection.
        m_main_camera->update_projection(60.0f, 0.1f, m_far, float(m_width) / float(m_height));
    }

    void key_pressed(int code) override;
    void key_released(int code) override;
    void mouse_pressed(int code) override;
    void mouse_released(int code) override;

private:
    inline bool create_shaders()
    {
        return true;
    }

    bool create_uniform_buffers();
    void create_descriptor_set_layouts();
    inline void create_descriptor_sets()
    {
        m_ds_transforms_main = m_vk_backend->allocate_descriptor_set(m_ds_layout_transforms);
        m_ds_transforms_shadow = m_vk_backend->allocate_descriptor_set(m_ds_layout_transforms);
        m_ds_shadow_sampler  = m_vk_backend->allocate_descriptor_set(m_ds_layout_shadow_sampler);
    }
    void write_descriptor_sets();
    void create_main_pipeline_state();
    void create_shadow_pipeline_state();

    bool load_object(std::string filename);
    bool        load_objects();
    inline void create_camera()
    {
        m_main_camera = std::make_unique<dw::Camera>(
            60.0f, 0.1f, m_far, float(m_width) / float(m_height), glm::vec3(0.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0, -1.0f));

        m_shadow_map->set_target(glm::vec3(-110.0f, 64.0f, 0.0f));
        m_shadow_map->set_direction(glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f)));
        m_shadow_map->set_backoff_distance(7000.0f);
        m_shadow_map->set_extents(1000.0f);
        m_shadow_map->set_near_plane(1.0f);
        m_shadow_map->set_far_plane(10000.0f);
    }

    void render_objects(dw::vk::CommandBuffer::Ptr cmd_buf, dw::vk::PipelineLayout::Ptr pipeline_layout);
    void begin_render_main(dw::vk::CommandBuffer::Ptr cmd_buf);
    void begin_render_shadow(dw::vk::CommandBuffer::Ptr cmd_buf);
    void render(dw::vk::CommandBuffer::Ptr cmd_buf);
    void update_uniforms(dw::vk::CommandBuffer::Ptr cmd_buf, bool shadow);
    void update_camera();

private:
    // GPU resources.
    size_t                           m_ubo_size_main;
    size_t                           m_ubo_size_shadow;
    dw::vk::GraphicsPipeline::Ptr    m_graphics_pipeline_main;
    dw::vk::GraphicsPipeline::Ptr    m_graphics_pipeline_shadow;
    dw::vk::PipelineLayout::Ptr      m_pipeline_layout_main;
    dw::vk::PipelineLayout::Ptr      m_pipeline_layout_shadow;
    dw::vk::DescriptorSetLayout::Ptr m_ds_layout_transforms;
    dw::vk::DescriptorSetLayout::Ptr m_ds_layout_shadow_sampler;
    dw::vk::DescriptorSet::Ptr       m_ds_transforms_main;
    dw::vk::DescriptorSet::Ptr       m_ds_transforms_shadow;
    dw::vk::DescriptorSet::Ptr       m_ds_shadow_sampler;
    dw::vk::Buffer::Ptr              m_ubo_transforms_main;
    dw::vk::Buffer::Ptr              m_ubo_transforms_shadow;

    // Camera.
    std::unique_ptr<dw::Camera> m_main_camera;

    // Camera controls.
    bool  m_mouse_look         = false;
    float m_heading_speed      = 0.0f;
    float m_sideways_speed     = 0.0f;
    float m_climbing_speed     = 0.0f;
    float m_camera_sensitivity = 0.05f;
    float m_camera_speed       = 0.2f;

    // Camera 
    float m_camera_x;
    float m_camera_y;
    float m_far = 2000.0f;

    // Assets.
    std::vector<dw::Mesh::Ptr> m_meshes;
    std::vector<RenderObject>  objects;

    // Uniforms.
    TransformsMain m_transforms_main;
    TransformsShadow m_transforms_shadow;

    // Shadow map
    std::unique_ptr<dw::ShadowMap> m_shadow_map;
    float m_shadow_map_size = 2048.0f;
    dw::vk::Sampler::Ptr m_shadow_map_sampler;
};