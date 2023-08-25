#version 450

layout (binding = 1) uniform sampler2D tex;

layout (location = 0) in vec3 normal;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 texCoord;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	outFragColor = texture(tex, texCoord);
}

