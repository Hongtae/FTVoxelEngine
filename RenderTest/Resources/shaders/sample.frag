#version 450

layout (binding = 0) uniform UBO 
{
	mat4 transform;
	vec3 lightDirection;
	vec3 lightColor;
} ubo;

layout (binding = 1) uniform sampler2D tex;

layout (location = 0) in vec3 normal;
layout (location = 1) in vec3 color;
layout (location = 2) in vec2 texCoord;

layout (location = 0) out vec4 outFragColor;

void main() 
{
	float lum = max(dot(normal, normalize(-ubo.lightDirection)), 0.0);
	outFragColor = texture(tex, texCoord) * vec4(ubo.lightColor * (0.3 + 0.7 * lum), 1.0);
}
