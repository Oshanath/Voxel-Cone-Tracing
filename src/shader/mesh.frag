#version 450

layout (location = 0) in vec3 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;
layout (location = 3) in vec4 FS_IN_FragPosLightSpace;

layout (location = 0) out vec3 FS_OUT_Color;

layout (set = 1, binding = 0) uniform sampler2D s_Diffuse;
layout (set = 1, binding = 1) uniform sampler2D s_Normal;
layout (set = 1, binding = 2) uniform sampler2D s_Metallic;
layout (set = 1, binding = 3) uniform sampler2D s_Roughness;

layout (set = 2, binding = 0) uniform sampler2D shadow_map;

float shadowCalculation(vec4 fragPosLightSpace)
{
	vec3 fragNDCCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
	fragNDCCoords = fragNDCCoords * 0.5 + 0.5;
	float closestDepth = texture(shadow_map, fragNDCCoords.xy).r;
	float currentDepth = fragNDCCoords.z;
	float shadow = currentDepth > closestDepth ? 1.0 : 0.0;
	return shadow;
}

void main()
{
    vec3 light_dir = normalize(vec3(-1.0, 1.0, 0.0));
	vec3 n = normalize(FS_IN_Normal);

	float lambert = max(0.0f, dot(n, light_dir));

    vec3 diffuse = texture(s_Diffuse, FS_IN_Texcoord).xyz;
	vec3 ambient = diffuse * 0.03;

	float shadowValue = shadowCalculation(FS_IN_FragPosLightSpace);
	vec3 color = diffuse * lambert * (1.0 - shadowValue) + ambient;
	//vec3 color = diffuse * lambert + ambient;

	// HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0 / 2.2));

    FS_OUT_Color = color;
}