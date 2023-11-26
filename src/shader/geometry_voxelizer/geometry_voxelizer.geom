#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Inputs from the vertex shader
layout (location = 0) in vec3 GS_IN_Pos[];
layout (location = 1) in vec2 GS_IN_Texcoord[];
layout (location = 2) in vec3 GS_IN_Normal[];

// Outputs for the fragment shader
layout (location = 0) out vec3 FS_IN_FragPos;
layout (location = 1) out vec2 FS_IN_Texcoord;
layout (location = 2) out vec3 FS_IN_Normal;

layout (set = 1, binding = 0) uniform PerFrameUBO 
{	
	mat4 view;
	mat4 projection;
	vec4 aabb_min;
	vec3 aabb_max;
} ubo;

void main()
{

    // Plane Normal
	const vec3 N = abs(cross(GS_IN_Pos[1] - GS_IN_Pos[0], GS_IN_Pos[2] - GS_IN_Pos[0]));

	for(int i = 0; i < 3; i++)
	{
		FS_IN_FragPos = GS_IN_Pos[i];
		FS_IN_Texcoord = GS_IN_Texcoord[i];
		FS_IN_Normal = GS_IN_Normal[i];

		if (N.z > N.x && N.z > N.y)
        {
            gl_Position = ubo.projection * ubo.view * vec4(FS_IN_FragPos.xyz, 1.0);
        }
        else if (N.x > N.y && N.x > N.z)
        {
			gl_Position = ubo.projection * ubo.view * vec4(FS_IN_FragPos.yzx, 1.0);
        }
        else
        {
			gl_Position = ubo.projection * ubo.view * vec4(FS_IN_FragPos.xzx, 1.0);
        }

        EmitVertex();
	}
	EndPrimitive();
}