#include "Voxelizer.h"
#include <gtc/matrix_transform.hpp>

Voxelizer::Voxelizer(glm::vec3 AABB_min, glm::vec3 AABB_max) :
    m_AABB_min(AABB_min), m_AABB_max(AABB_max),
    m_center((AABB_min + AABB_max) / 2.0f),
    m_x_length(std::abs(AABB_min.x - AABB_max.x)),
    m_y_length(std::abs(AABB_min.y - AABB_max.y)),
    m_z_length(std::abs(AABB_min.z - AABB_max.z)),
    m_cam_pos(m_center + glm::vec3(m_x_length / 2 + 100.0f, 0.0f, 0.0f)),
    m_cam_forward(glm::vec3(1.0f, 0.0f, 0.0f)),
    m_view(glm::lookAt(m_cam_pos, m_center, glm::vec3(0.0f, 1.0f, 0.0f))),
    m_proj(glm::ortho(-m_z_length / 2, m_z_length / 2, -m_y_length / 2, m_y_length / 2, 0.0f, m_x_length))
{
}

Voxelizer::~Voxelizer()
{
}

glm::vec3 Voxelizer::get_center() const
{
    return m_center;
}

glm::mat4 Voxelizer::get_view() const
{
    return m_view;
}

glm::mat4 Voxelizer::get_proj() const
{
    return m_proj;
}

glm::vec3 Voxelizer::get_cam_pos() const
{
    return m_cam_pos;
}
