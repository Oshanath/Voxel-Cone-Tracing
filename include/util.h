#pragma once
#include <application.h>

// Push constant data structure (model matrix)
struct MeshPushConstants
{
        glm::mat4 model;
        glm::int32_t triangle_count;
};

struct AABB
{
    glm::vec3 min;
	glm::vec3 max;
};
