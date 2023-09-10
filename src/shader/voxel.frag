#version 450

layout (location = 0) in vec4 FS_IN_FragPos;
layout (location = 1) in vec2 FS_IN_Texcoord;
layout (location = 2) in vec3 FS_IN_Normal;

layout (set = 1, binding = 0) uniform PerFrameUBO 
{	
	mat4 view;
	mat4 projection;
	vec4 aabb_min;
	vec3 aabb_max;
} ubo;

layout(set = 2, binding = 0, rgba8) uniform image3D voxelTexture;

int get_index(float boundary1, float boundary2, float value, int voxels_per_side)
{
	float maxBoundary = max(boundary1, boundary2);
	float minBoundary = min(boundary1, boundary2);

	return int((value - minBoundary) / (maxBoundary - minBoundary) * voxels_per_side);
}

void main()
{
	vec3 _min = ubo.aabb_min.xyz;
	vec3 _max = ubo.aabb_max.xyz;
	int voxels_per_side = int(ubo.aabb_min.w);
	mat4 grid_space_transformation = ubo.projection * ubo.view;

	vec4 frag_grid_space = grid_space_transformation * FS_IN_FragPos;
	vec4 min_grid_space = grid_space_transformation * vec4(_min, 1.0);
	vec4 max_grid_space = grid_space_transformation * vec4(_max, 1.0);

	ivec3 voxel_coordinate = ivec3(
		get_index(min_grid_space.x, max_grid_space.x, frag_grid_space.x, voxels_per_side),
		get_index(min_grid_space.y, max_grid_space.y, frag_grid_space.y, voxels_per_side),
		get_index(min_grid_space.z, max_grid_space.z, frag_grid_space.z, voxels_per_side)
	);

	const uvec4 voxel = uvec4(1.0, 1.0, 1.0, 1.0);
    imageStore(voxelTexture, voxel_coordinate, voxel);
}