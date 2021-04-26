#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 pos;

vec2 positions[6] = vec2[](
    vec2(0.5, 0.5),
    vec2(0.5, -0.5),
    vec2(-0.5, 0.5),
    vec2(0.5, -0.5),
    vec2(-0.5, -0.5),
    vec2(-0.5, 0.5)
);


void main() {
    gl_Position = vec4(positions[gl_VertexIndex]*2, 0.5, 1.0);
    outUV = positions[gl_VertexIndex];
    pos=gl_Position;
}