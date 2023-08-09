#version 450

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (binding = 0) uniform UBO 
{
	mat4 transform;
	vec3 lightDirection;
	vec3 lightColor;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outTexCoord;

out gl_PerVertex 
{
    vec4 gl_Position;   
};

void main() 
{
	gl_Position = ubo.transform * vec4(inPos, 1.0);
	outNormal = normalize(mat3(ubo.transform) * inNormal);
	outColor = vec3(1, 1, 1);
	outTexCoord = inTexCoord;
}
