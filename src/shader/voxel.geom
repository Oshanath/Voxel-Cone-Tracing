#version 450

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

// Inputs from the vertex shader
layout (location = 0) in vec4 GS_IN_FragPos[];
layout (location = 1) in vec2 GS_IN_Texcoord[];
layout (location = 2) in vec3 GS_IN_Normal[];

// Outputs for the fragment shader
layout (location = 0) out vec4 FS_IN_FragPos;
layout (location = 1) out vec2 FS_IN_Texcoord;
layout (location = 2) out vec3 FS_IN_Normal;

void main()
{
	for(int i = 0; i < gl_in.length(); i++)
	{

		// Pass values from vertex shader to fragment shader
        FS_IN_FragPos = GS_IN_FragPos[i];
        FS_IN_Texcoord = GS_IN_Texcoord[i];
        FS_IN_Normal = GS_IN_Normal[i];

		gl_Position = gl_in[i].gl_Position;
		EmitVertex();
	}
	EndPrimitive();
}