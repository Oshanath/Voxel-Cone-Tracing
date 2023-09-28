#version 450

#extension GL_KHR_vulkan_glsl:enable

layout(location = 0) in vec4 VS_IN_Position;

layout (set = 0, binding = 0) uniform PerFrameUBO 
{
	mat4 view;
	mat4 projection;
} ubo;

layout (set = 1, binding = 0) uniform InstanceBuffer
{
	vec4 position[2];
} instanceBuffer;

void main() 
{
    // Transform position into world space
	vec4 world_pos =  vec4(VS_IN_Position.xyz, 1.0) + vec4(instanceBuffer.position[gl_InstanceIndex]);

    // Transform world position into clip space
	gl_Position = ubo.projection * ubo.view * world_pos;
}