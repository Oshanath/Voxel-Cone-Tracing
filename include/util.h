#pragma once
#include <application.h>

// Push constant data structure (model matrix)
struct MeshPushConstants
{
    DW_ALIGNED(16)
        glm::mat4 model;
};