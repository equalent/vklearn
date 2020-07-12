#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "triangle.glsli"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

void main() {
	vec3 pos = inPosition;
	float rad = -radians(ub.angle);
	mat3 rotMat = mat3( cos(rad), -sin(rad), 0.0,
						sin(rad), cos(rad), 0.0,
						0.0, 0.0, 1.0);
	
	pos = rotMat * pos;
	
    gl_Position = vec4(pos, 1.0);
    fragColor = inColor;
}