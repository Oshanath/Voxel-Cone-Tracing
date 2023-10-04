#version 450

layout (location = 0) in vec3 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (set = 1, binding = 0) uniform PerFrameUBO 
{	
	mat4 view;
	mat4 projection;
	vec4 aabb_min;
	vec4 aabb_max;
} ubo;

layout(set = 2, binding = 0, rgba8) uniform image3D voxelTexture;

int get_index(float boundary1, float boundary2, float value, int voxels_per_side)
{
	return int((value - boundary1) / (boundary2 - boundary1) * voxels_per_side);
}

void main()
{
	vec3 _min = ubo.aabb_min.xyz;
	vec3 _max = ubo.aabb_max.xyz;
	int voxels_per_side = imageSize(voxelTexture).x;
	float voxel_width = (_max.x - _min.x) / float(voxels_per_side);

	const vec4 voxel_value = vec4(1.0, 1.0, 1.0, 1.0);
	imageStore(voxelTexture, ivec3((FS_IN_FragPos - _min) / voxel_width), voxel_value);
}