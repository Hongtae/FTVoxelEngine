#version 450

#define VISUAL_MODE_RAYCAST			0
#define VISUAL_MODE_LOD				1
#define VISUAL_MODE_SSAO			2
#define VISUAL_MODE_NORMAL			3
#define VISUAL_MODE_ALBEDO			4
#define VISUAL_MODE_COMPOSITION		5

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;

layout (location=0) in vec2 inUV;
layout (location=0) out vec4 outFragColor;

layout (push_constant) uniform Constants {
	int drawMode;
} pc;


void main() {

	vec3 fragPos = texture(samplerPosition, inUV).rgb;
	vec3 normal = normalize(texture(samplerNormal, inUV).rgb * 2.0 - 1.0);
	vec4 albedo = texture(samplerAlbedo, inUV);
	float ssao = texture(samplerSSAO, inUV).r;

	vec3 lightPos = vec3(0.0);
	vec3 L = normalize(lightPos - fragPos);
	float NdotL = max(0.5, dot(normal, L));

	switch (pc.drawMode) {
	case VISUAL_MODE_SSAO:
		outFragColor = vec4(ssao.rrr, 1.0);
		break;
	case VISUAL_MODE_NORMAL:
		outFragColor = vec4(normal, 1.0);
		break;
	case VISUAL_MODE_ALBEDO:
		outFragColor = albedo;
		break;
	case VISUAL_MODE_RAYCAST:
		outFragColor = vec4(albedo.rgb, 1.0);
		break;
	case VISUAL_MODE_LOD:
		outFragColor = vec4(albedo.aaa, 1.0);
		break;
	case VISUAL_MODE_COMPOSITION:
		vec3 baseColor = albedo.rgb * NdotL;
		outFragColor = vec4(baseColor * ssao, albedo.a);
		break;
	default:
	    outFragColor = vec4(0);
		break;
	}
}
