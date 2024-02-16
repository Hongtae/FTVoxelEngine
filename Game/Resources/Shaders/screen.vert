#version 450

layout (location = 0) out vec2 outUV;

out gl_PerVertex
{
	vec4 gl_Position;
};

void main() 
{
	outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
	vec2 pos = vec2(outUV.x * 2.0 - 1.0, 1.0 - outUV.y * 2.0);
	gl_Position = vec4(pos, 0.0, 1.0);
}
