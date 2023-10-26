#version 450

layout (push_constant) uniform Constants {
	layout(row_major) mat4 model;
	layout(row_major) mat4 vp;
	vec3 lightDir;
	vec3 lightColor;
	vec3 ambientColor;
} pc;

layout (binding = 0) uniform Material {
	vec4 baseColor;
	float metallic;
	float roughness;
} material;

layout (binding = 1) uniform sampler2D tex;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inTexCoord;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    float lum = max(dot(inNormal, normalize(-pc.lightDir)), 0.0);
	vec3 color = mix(pc.ambientColor, pc.lightColor * lum, 0.7);
    outFragColor = texture(tex, inTexCoord) * material.baseColor * vec4(color, 1.0);
}
