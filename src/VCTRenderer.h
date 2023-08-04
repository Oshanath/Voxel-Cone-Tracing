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

// Uniform buffer data structure.
struct Transforms
{
    DW_ALIGNED(16)
    glm::mat4 model;
    DW_ALIGNED(16)
    glm::mat4 view;
    DW_ALIGNED(16)
    glm::mat4 projection;
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
        m_main_camera->update_projection(60.0f, 0.1f, 10000.0f, float(m_width) / float(m_height));
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

    bool create_uniform_buffer();
    void create_descriptor_set_layout();
    inline void create_descriptor_set()
    {
        m_per_frame_ds = m_vk_backend->allocate_descriptor_set(m_per_frame_ds_layout);
    }
    void write_descriptor_set();
    void create_pipeline_state();

    bool load_mesh();
    inline void create_camera()
    {
        m_main_camera = std::make_unique<dw::Camera>(
            60.0f, 0.1f, 1000.0f, float(m_width) / float(m_height), glm::vec3(0.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0, -1.0f));
    }

    void render(dw::vk::CommandBuffer::Ptr cmd_buf);
    void update_uniforms(dw::vk::CommandBuffer::Ptr cmd_buf);
    void update_camera();

private:
    // GPU resources.
    size_t                           m_ubo_size;
    dw::vk::GraphicsPipeline::Ptr    m_pso;
    dw::vk::PipelineLayout::Ptr      m_pipeline_layout;
    dw::vk::DescriptorSetLayout::Ptr m_per_frame_ds_layout;
    dw::vk::DescriptorSet::Ptr       m_per_frame_ds;
    dw::vk::Buffer::Ptr              m_ubo;

    // Camera.
    std::unique_ptr<dw::Camera> m_main_camera;

    // Camera controls.
    bool  m_mouse_look         = false;
    float m_heading_speed      = 0.0f;
    float m_sideways_speed     = 0.0f;
    float m_climbing_speed     = 0.0f;
    float m_camera_sensitivity = 0.05f;
    float m_camera_speed       = 0.2f;

    // Camera orientation.
    float m_camera_x;
    float m_camera_y;

    // Assets.
    dw::Mesh::Ptr m_mesh;

    // Uniforms.
    Transforms m_transforms;
};