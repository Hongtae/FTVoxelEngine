#version 450

#define VISUAL_MODE_RAYCAST			0
#define VISUAL_MODE_SSAO			1
#define VISUAL_MODE_ALBEDO			2
#define VISUAL_MODE_COMPOSITION		3

layout (binding = 0) uniform sampler2D samplerPosition;
layout (binding = 1) uniform sampler2D samplerNormal;
layout (binding = 2) uniform sampler2D samplerAlbedo;
layout (binding = 3) uniform sampler2D samplerSSAO;

layout (location=0) in vec2 texUV;
layout (location=0) out vec4 outFragColor;

layout (push_constant) uniform Constants {
	int drawMode;
} pc;

void main() {
	switch (pc.drawMode) {
	case VISUAL_MODE_SSAO:
		float ssao = texture(samplerSSAO, texUV).r;
		outFragColor = vec4(ssao.rrr, 1.0);
		break;
	case VISUAL_MODE_ALBEDO:
	case VISUAL_MODE_RAYCAST:
		outFragColor = texture(samplerAlbedo, texUV);
		break;
	case VISUAL_MODE_COMPOSITION:
		outFragColor = texture(samplerAlbedo, texUV);
		break;
	default:
	    outFragColor = vec4(0);
		break;
	}
}
