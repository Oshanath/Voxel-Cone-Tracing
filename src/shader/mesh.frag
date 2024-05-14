#version 450

layout (location = 0) in vec4 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (location = 0) out vec3 FS_OUT_Color;

layout (set = 0, binding = 0) uniform sampler2D s_Diffuse;
layout (set = 0, binding = 1) uniform sampler2D s_Normal;
layout (set = 0, binding = 2) uniform sampler2D s_Metallic;
layout (set = 0, binding = 3) uniform sampler2D s_Roughness;

layout (set = 2, binding = 0) uniform sampler2D shadow_map;

layout (set = 1, binding = 0) uniform PerFrameUBO 
{	
	mat4 view;
	mat4 projection;
	mat4 lightSpaceMatrix;
} ubo;

layout (set = 3, binding = 0) uniform LightsUBO 
{	
	vec4 position;
	vec3 direction;
	int type;
	vec3 color;
	float intensity;
} lights;

layout( push_constant ) uniform constants{
	mat4 model;
} pc;

float ambient = 0.03;

float textureProj(vec4 shadowCoord, vec2 off)
{
	float shadow = 1.0;
	if ( shadowCoord.z > -1.0 && shadowCoord.z < 1.0 ) 
	{
		float dist = texture( shadow_map, shadowCoord.st + off ).r;
		if ( shadowCoord.w > 0.0 && dist < shadowCoord.z ) 
		{
			shadow = ambient;
		}
	}
	return shadow;
}

float filterPCF(vec4 sc)
{
	ivec2 texDim = textureSize(shadow_map, 0);
	float scale = 1.5;
	float dx = scale * 1.0 / float(texDim.x);
	float dy = scale * 1.0 / float(texDim.y);

	float shadowFactor = 0.0;
	int count = 0;
	int range = 1;
	
	for (int x = -range; x <= range; x++)
	{
		for (int y = -range; y <= range; y++)
		{
			shadowFactor += textureProj(sc, vec2(dx*x, dy*y));
			count++;
		}
	
	}
	return shadowFactor / count;
}

void main()
{
    vec3 light_dir = lights.direction;
	vec3 n = normalize(FS_IN_Normal);

	float lambert = max(0.0f, dot(n, -light_dir));

    vec3 diffuse = texture(s_Diffuse, FS_IN_Texcoord).xyz;
	vec3 ambient = diffuse * ambient;

	vec4 FragPosLightSpace = ubo.lightSpaceMatrix * FS_IN_FragPos;

	vec4 fragNDCCoords = FragPosLightSpace / FragPosLightSpace.w;

	fragNDCCoords.xy = fragNDCCoords.xy * 0.5 + 0.5;

	float closestDepth = texture(shadow_map, fragNDCCoords.xy).r;
	float currentDepth = fragNDCCoords.z;
	//float shadowValue = currentDepth > closestDepth ? 1.0 : 0.0;
	float shadowValue = filterPCF(fragNDCCoords);

	vec3 color = diffuse * lambert * shadowValue + ambient;

	// HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correct
    color = pow(color, vec3(1.0 / 2.2));

	FS_OUT_Color = color;
}