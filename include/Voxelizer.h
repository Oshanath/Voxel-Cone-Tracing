#pragma once
#include <glm.hpp>

class Voxelizer
{
public:

	Voxelizer(glm::vec3 AABB_min, glm::vec3 AABB_max);
	~Voxelizer();

	glm::vec3 get_center() const;
	glm::mat4 get_view() const;
	glm::mat4 get_proj() const;
	glm::vec3 get_cam_pos() const;

private:
	glm::vec3 m_AABB_min;
	glm::vec3 m_AABB_max;
	glm::vec3 m_center;
	float m_x_length;
	float m_y_length;
	float m_z_length;
	glm::vec3 m_cam_pos;
	glm::vec3 m_cam_forward;
	glm::mat4 m_view;
	glm::mat4 m_proj;
};