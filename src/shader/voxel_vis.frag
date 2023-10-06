#version 450

layout (location = 0) out vec3 FS_OUT_Color;

layout(std140, set=2, binding = 0) buffer InstanceColorBuffer {
   vec4 colors[];
};

// Take instance index as a flat input
layout (location = 1) flat in int instanceIndex;

void main()
{
	FS_OUT_Color = vec3(colors[instanceIndex].xyz);
}