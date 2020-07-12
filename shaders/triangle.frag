#version 450
#extension GL_ARB_separate_shader_objects : enable
#include "triangle.glsli"

layout(location = 0) in vec3 inColor;

layout(location = 0) out vec4 outColor;

void main() {
	vec4 oc = vec4(inColor, 1.0f);
	oc.r *= mapRange(ub.speed, 0, 1000, 0, 1);
	outColor = oc;
}