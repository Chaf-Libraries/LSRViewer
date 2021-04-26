#version 450 core                                                                             

layout(triangles, fractional_odd_spacing, cw) in;       

// Output
struct PosPatch
{
    vec3 pos[9];
};

layout (location = 0) in patch PosPatch inPosPatch;
layout (location = 9) in vec3 inNormal[];
layout (location = 10) in vec3 inColor[];
layout (location = 11) in vec2 inUV[];
layout (location = 12) in vec4 inTangent[];
layout (location = 13) in mat3 inTBN[];

layout (location = 0) out vec3 outPos;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec3 outColor;
layout (location = 3) out vec2 outUV;
layout (location = 4) out vec3 outViewVec;
layout (location = 5) out vec3 outLightVec;
layout (location = 6) out vec4 outTangent;
layout (location = 7) out mat3 outTBN;

// Uniform buffer
layout (set = 0, binding = 0) uniform UBOScene 
{
	mat4 projection;
	mat4 view;
	vec4 lightPos;
	vec4 viewPos;
    vec4 frustum[6];
	vec4 range;
} uboScene;

// Push constant
layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

vec3 interpolation(vec3 v1, vec3 v2, vec3 v3)
{
    return gl_TessCoord.x * v1 + gl_TessCoord.y * v2 + gl_TessCoord.z * v3;
}

vec2 interpolation(vec2 v1, vec2 v2, vec2 v3)
{
    return gl_TessCoord.x * v1 + gl_TessCoord.y * v2 + gl_TessCoord.z * v3;
}

vec4 interpolation(vec4 v1, vec4 v2, vec4 v3)
{
    return gl_TessCoord.x * v1 + gl_TessCoord.y * v2 + gl_TessCoord.z * v3;
}

mat3 interpolation(mat3 v1, mat3 v2, mat3 v3)
{
    return gl_TessCoord.x * v1 + gl_TessCoord.y * v2 + gl_TessCoord.z * v3;
}

void main()                                                                                    
{                                                                                               
  
    float u = gl_TessCoord.x;                                                                 
    float v = gl_TessCoord.y;                                                                  
    float w = gl_TessCoord.z;

    outNormal = interpolation(inNormal[0], inNormal[1], inNormal[2]);
    outColor = interpolation(inColor[0], inColor[1], inColor[2]);
    outUV = interpolation(inUV[0], inUV[1], inUV[2]);
    outTangent = interpolation(inTangent[0], inTangent[1], inTangent[2]);
    outTBN = interpolation(inTBN[0], inTBN[1], inTBN[2]);

    // outUV = u * inUV[0] + v * inUV[1] + w * inUV[2];
    // //outTBN =u* inTBN[0] + v * inTBN[1] + w * inTBN[2];

    // outViewVec = u* inViewVec[0] + v * inViewVec[1] + w * inViewVec[2];
    // outLightVec = u* inLightVec[0] + v * inLightVec[1] + w * inLightVec[2];
    // outNormal = u* inNormal[0] + v * inNormal[1] + w * inNormal[2];
    // outTangent = u* inTangent[0] + v * inTangent[1] + w * inTangent[2];
  
    vec3 N = normalize(outNormal);
	vec3 T = normalize(outTangent.xyz);
	vec3 B = cross(outNormal, outTangent.xyz) * outTangent.w;
	outTBN = mat3(T, B, N);

    
    vec3 P1= inPosPatch.pos[0] * u + inPosPatch.pos[1] * v + inPosPatch.pos[2] * w;
    vec3 P2= inPosPatch.pos[3] * u + inPosPatch.pos[4] * v + inPosPatch.pos[5] * w;
    vec3 P3= inPosPatch.pos[6] * u + inPosPatch.pos[7] * v + inPosPatch.pos[8] * w;

    outPos = P1 * u+ P2 * v + P3 * w;
    //outPos = inPosPatch1[0] * u+ inPosPatch2[1] * v + inPosPatch1[2] * w;
	
	// float h = 0;//texture(height_texture, te_out.TEX).r;
	// // vec3 N = normalize(outTBN[2]);
	// //0.02是置换强度,可以调整
	// //outPos += 0.02*(h * 2 - 1) * N;
    //     gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +
    //               (gl_TessCoord.y * gl_in[1].gl_Position) +
    //               (gl_TessCoord.z * gl_in[2].gl_Position);
    // gl_Position = uboScene.projection * uboScene.view * primitive.model* gl_Position;



    //outPos = P1;  
    gl_Position = vec4(outPos, 1.0);
	gl_Position = uboScene.projection * uboScene.view * primitive.model* gl_Position;
    gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;
	
    //outNormal = mat3(primitive.model) * outNormal;
	vec4 pos = primitive.model * vec4(outPos, 1.0);
	outLightVec = uboScene.lightPos.xyz - pos.xyz;
	outViewVec = uboScene.viewPos.xyz - pos.xyz;
}  