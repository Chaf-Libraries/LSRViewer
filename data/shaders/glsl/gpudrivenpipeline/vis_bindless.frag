#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_nonuniform_qualifier : require

layout (set = 0, binding = 0) uniform sampler2D textureArray[];

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 pos;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConsts {
	int textureNum;
};

void main() {

    // for(int i=0; i < textureNum; i++)
    // {
    int d = int(sqrt(textureNum))+1;
    vec2 uv = (inUV*2+1)/2;
    int idx=int(int(uv.x*d)*(d-1)+uv.y*d);
        
    if(idx < textureNum)
    {
        outColor = texture(textureArray[nonuniformEXT(idx)], uv*d);
    }
    else
    {
        outColor=vec4(0.0,0.0,0.0,1.0);
    }
    // }
    // outColor = vec4(texture(input_texture, (inUV*2+1)/2).xyz, 1.0);
    //  outColor = vec4((inUV*2+1) * 0.5,0,1);
}