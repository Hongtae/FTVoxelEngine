#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (push_constant) uniform Constants 
{
	mat4 projection;
	mat4 model;
	mat4 view;
} pc;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outTexCoord;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	mat4 transform = pc.projection * pc.view * pc.model;
	gl_Position = transform * vec4(inPos, 1.0);
	outNormal = normalize(inNormal * mat3(transform));
	outColor = vec3(1, 1, 1);
	outTexCoord = inTexCoord;
}
