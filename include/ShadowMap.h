#include <memory>
#include <ogl.h>
#include <glm.hpp>
#include <vk.h>

class ShadowMap
{
private:
    dw::vk::Image::Ptr       m_image;
    dw::vk::ImageView::Ptr   m_image_view;
    dw::vk::Framebuffer::Ptr m_framebuffer;
    dw::vk::RenderPass::Ptr  m_render_pass;

    glm::vec3 m_light_target = glm::vec3(0.0f);
    glm::vec3 m_light_direction = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 m_light_color = glm::vec3(1.0f);
    glm::mat4 m_view;
    glm::mat4 m_projection;
    float     m_near_plane = 1.0f;
    float     m_far_plane = 1000.0f;
    float     m_backoff_distance = 200.0f;
    uint32_t  m_width = 1024;
    uint32_t  m_height = 1024;


public:
    void update();
    float     m_extents = 75.0f;

    ShadowMap(dw::vk::Backend::Ptr backend, uint32_t m_width, uint32_t m_height);
    ~ShadowMap();
    void begin_render(dw::vk::CommandBuffer::Ptr cmd_buf);
    void end_render(dw::vk::CommandBuffer::Ptr cmd_buf);

    void gui();
    void set_direction(const glm::vec3& d);
    void set_target(const glm::vec3& t);
    void set_color(const glm::vec3& c);
    void set_near_plane(const float& n);
    void set_far_plane(const float& f);
    void set_extents(const float& e);
    void set_backoff_distance(const float& d);

    inline dw::vk::Image::Ptr image()
    {
        return m_image;
    };
    inline dw::vk::ImageView::Ptr   image_view() { return m_image_view; }
    inline dw::vk::Framebuffer::Ptr framebuffer() { return m_framebuffer; }
    inline dw::vk::RenderPass::Ptr  render_pass() { return m_render_pass; }

    inline glm::vec3 direction() { return m_light_direction; }
    inline glm::vec3 target() { return m_light_target; }
    inline glm::vec3 color() { return m_light_color; }
    inline float     near_plane() { return m_near_plane; }
    inline float     far_plane() { return m_far_plane; }
    inline float     extents() { return m_extents; }
    inline float     backoff_distance() { return m_backoff_distance; }
    inline glm::mat4 view() { return m_view; }
    inline glm::mat4 projection() { return m_projection; }
};