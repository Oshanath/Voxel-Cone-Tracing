#pragma once
#include <application.h>

// Push constant data structure (model matrix)
struct MeshPushConstants
{
    DW_ALIGNED(16)
        glm::mat4 model;
};

// Pipeline selector
enum PipelineType
{
	PIPELINE_MAIN,
    PIPELINE_SHADOW,
    PIPELINE_VOXELIZER
};

struct AABB
{
    glm::vec3 min;
	glm::vec3 max;
};
