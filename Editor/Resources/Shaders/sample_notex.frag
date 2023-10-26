#version 450

layout (push_constant) uniform Constants 
{
	layout(row_major) mat4 model;
	layout(row_major) mat4 vp;
	vec3 lightDir;
	vec3 lightColor;
	vec3 ambientColor;
} pc;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec4 inColor;
layout (location = 0) out vec4 outFragColor;

void main() 
{
    float lum = max(dot(inNormal, normalize(-pc.lightDir)), 0.0);
	vec3 color = mix(pc.ambientColor, pc.lightColor * lum, 0.7);
    outFragColor = inColor * vec4(color, 1.0);
}
