#version 450

layout(location = 0) in vec4 VS_IN_Position;

layout( push_constant ) uniform constants
{
	mat4 model;
} pc;

layout (set = 1, binding = 0) uniform PerFrameUBO 
{
	mat4 view;
	mat4 projection;
} ubo;

void main() 
{
    // Transform position into world space
	vec4 world_pos = pc.model * vec4(VS_IN_Position.xyz, 1.0);

    // Transform world position into clip space
	gl_Position = ubo.projection * ubo.view * world_pos;
}