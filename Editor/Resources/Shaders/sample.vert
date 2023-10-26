#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (push_constant) uniform Constants {
	layout(row_major) mat4 model;
	layout(row_major) mat4 vp;
	vec3 lightDir;
	vec3 lightColor;
	vec3 ambientColor;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec2 outTexCoord;

out gl_PerVertex {
    vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	mat4 tm = pc.model * pc.vp;
	outNormal = normalize((vec4(inNormal, 0) * pc.model).xyz);
	outTexCoord = inTexCoord;
	gl_Position = vec4(inPos, 1) * tm;
	gl_PointSize = 1;
}
