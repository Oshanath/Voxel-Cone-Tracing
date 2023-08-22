#version 450

layout (location = 0) in vec4 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (location = 0) out vec3 FS_OUT_Color;

layout (set = 1, binding = 0) uniform sampler2D s_Diffuse;
layout (set = 1, binding = 1) uniform sampler2D s_Normal;
layout (set = 1, binding = 2) uniform sampler2D s_Metallic;
layout (set = 1, binding = 3) uniform sampler2D s_Roughness;

layout (set = 2, binding = 0) uniform sampler2D shadow_map;

layout (set = 0, binding = 0) uniform PerFrameUBO 
{
	mat4 view;
	mat4 projection;
	mat4 lightSpaceMatrix;
} ubo;

void main()
{
    vec3 light_dir = normalize(vec3(-0.1, 1.0, 0.0));
	vec3 n = normalize(FS_IN_Normal);

	float lambert = max(0.0f, dot(n, light_dir));

    vec3 diffuse = texture(s_Diffuse, FS_IN_Texcoord).xyz;
	vec3 ambient = diffuse * 0.03;

	vec4 FragPosLightSpace = ubo.lightSpaceMatrix * FS_IN_FragPos;

	//vec3 fragNDCCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
	vec3 fragNDCCoords = FragPosLightSpace.xyz;

	fragNDCCoords.xy = fragNDCCoords.xy * 0.5 + 0.5;
	//fragNDCCoords.y = 1-fragNDCCoords.y;

	float closestDepth = texture(shadow_map, fragNDCCoords.xy).r;
	float currentDepth = fragNDCCoords.z;
	float shadowValue = currentDepth > closestDepth ? 1.0 : 0.0;

	vec3 color = diffuse * lambert * (1.0 - shadowValue) + ambient;
	//vec3 color = diffuse * lambert + ambient;

	// HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0 / 2.2));

	FS_OUT_Color = color;
	//float c = texture(shadow_map, fragNDCCoords.xy).r;
	//FS_OUT_Color = vec4(closestDepth, currentDepth, fragNDCCoords.xy);
	
}