#version 450

#define VISUAL_MODE_RAYCAST			0
#define VISUAL_MODE_SSAO			1
#define VISUAL_MODE_NORMAL			2
#define VISUAL_MODE_ALBEDO			3
#define VISUAL_MODE_COMPOSITION		4

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;

layout (location=0) in vec2 inUV;
layout (location=0) out vec4 outFragColor;

layout (push_constant) uniform Constants {
	int drawMode;
} pc;

vec4 composite() {
	vec3 fragPos = texture(samplerPosition, inUV).rgb;
	vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
	vec4 albedo = texture(samplerAlbedo, inUV);
	float ssao = texture(samplerSSAO, inUV).r;

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	vec3 baseColor = albedo.rgb * NdotL;

	vec4 result = vec4(baseColor * ssao, albedo.a);
	return result;
}

void main() {
	switch (pc.drawMode) {
	case VISUAL_MODE_SSAO:
		float ssao = texture(samplerSSAO, inUV).r;
		outFragColor = vec4(ssao.rrr, 1.0);
		break;
	case VISUAL_MODE_NORMAL:
		outFragColor = texture(samplerNormal, inUV);
		break;
	case VISUAL_MODE_ALBEDO:
	case VISUAL_MODE_RAYCAST:
		outFragColor = texture(samplerAlbedo, inUV);
		break;
	case VISUAL_MODE_COMPOSITION:
		outFragColor = composite();
		break;
	default:
	    outFragColor = vec4(0);
		break;
	}
}
