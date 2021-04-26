#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_texture_lod : require

layout (set = 0, binding = 0) uniform sampler2D input_texture;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 pos;

layout(location = 0) out vec4 outColor;

void main() {
    // outColor = vec4(texture(input_texture, (inUV*2+1)/2).xyz, 1.0);
    outColor = textureLod(input_texture, (inUV*2+1) * 0.5, 0);
    //  outColor = vec4((inUV*2+1) * 0.5,0,1);
}