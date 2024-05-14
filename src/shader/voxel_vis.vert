#version 450

#extension GL_KHR_vulkan_glsl:enable

layout(location = 0) in vec4 VS_IN_Position;
layout(location = 1) in vec4 VS_IN_Texcoord;
layout(location = 2) in vec4 VS_IN_Normal;
layout(location = 3) in vec4 VS_IN_Tangent;
layout(location = 4) in vec4 VS_IN_Bitangent;

layout (set = 0, binding = 0) uniform PerFrameUBO 
{
	mat4 model;
	mat4 view;
	mat4 projection;
} ubo;

layout(std140, set=1, binding = 0) readonly buffer InstanceBuffer {
   vec4 positions[];
};

// output instance index to fragment shader as a flat value
layout (location = 1) flat out int instanceIndex;

void main() 
{
    // Transform position into world space
	vec4 world_pos =  ubo.model * vec4(VS_IN_Position.xyz, 1.0) + vec4(positions[gl_InstanceIndex]);

    // Transform world position into clip space
	gl_Position = ubo.projection * ubo.view * world_pos;

	instanceIndex = gl_InstanceIndex;
}