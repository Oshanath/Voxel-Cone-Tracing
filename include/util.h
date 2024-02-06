#pragma once
#include <application.h>

// Push constant data structure (model matrix)
struct MeshPushConstants
{
    DW_ALIGNED(16)
        glm::mat4 model;
    DW_ALIGNED(16)
        glm::int32_t start_index;
};

struct AABB
{
    glm::vec3 min;
	glm::vec3 max;
};
