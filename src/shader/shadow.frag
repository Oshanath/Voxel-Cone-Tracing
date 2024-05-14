#version 450

layout (location = 0) in vec3 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (set = 0, binding = 0) uniform sampler2D s_Diffuse;
layout (set = 0, binding = 1) uniform sampler2D s_Normal;
layout (set = 0, binding = 2) uniform sampler2D s_Metallic;
layout (set = 0, binding = 3) uniform sampler2D s_Roughness;

void main()
{
    
}