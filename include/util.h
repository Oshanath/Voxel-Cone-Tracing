#pragma once
#include <application.h>

// Push constant data structure (model matrix)
struct MeshPushConstants
{
        glm::mat4 model;
        glm::int32_t triangle_count;
		float occlusionDecayFactor;
		VkBool32 ambientOcclusionEnabled;
		VkBool32 occlusionVisualizationEnabled;
		float surfaceOffset;
		float coneCutoff;
};

struct AABB
{
    glm::vec3 min;
	glm::vec3 max;
};
