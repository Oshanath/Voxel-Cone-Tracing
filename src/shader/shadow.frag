#version 450

layout (location = 0) in vec3 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (location = 0) out vec3 FS_OUT_Color;

layout (set = 1, binding = 0) uniform sampler2D s_Diffuse;
layout (set = 1, binding = 1) uniform sampler2D s_Normal;
layout (set = 1, binding = 2) uniform sampler2D s_Metallic;
layout (set = 1, binding = 3) uniform sampler2D s_Roughness;

void main()
{
    float depth = FS_IN_FragPos.z / 1000;
    FS_OUT_Color = vec3(depth, 0.0, 0.0);
}