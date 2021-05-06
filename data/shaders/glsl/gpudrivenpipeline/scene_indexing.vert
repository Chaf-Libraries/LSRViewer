#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shader_draw_parameters: enable

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (location = 5) in uint inIndex;

layout (set = 0, binding = 0) uniform UBOScene 
{
	mat4 projection;
	mat4 view;
	vec4 lightPos;
	vec4 viewPos;
    vec4 frustum[6];
	vec4 range;
} uboScene;

struct ObjectData 
{
	int baseColorTextureIndex;
	int normalTextureIndex;
	int emissiveTextureIndex;
	int occlusionTextureIndex;
	int metallicRoughnessTextureIndex;
	float metallicFactor;
	float roughnessFactor;
	int alphaMode;
	float alphaCutOff;
	uint doubleSided;

	// Parameter
	vec4 baseColorFactor;
	vec3 emissiveFactor;

	mat4 model;
};

layout (set = 0, binding = 1) buffer ObjectBuffer 
{
   ObjectData objectData[ ];
};

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec3 outLightVec;
layout (location = 5) out vec4 outTangent;
layout (location = 6) flat out uint outIndex;


void main() 
{
	outNormal = inNormal;
	outColor = inColor;
	outUV = inUV;
	outTangent = inTangent;
	outIndex = gl_InstanceIndex;
	gl_Position = uboScene.projection * uboScene.view *objectData[gl_InstanceIndex].model*  vec4(inPos.xyz, 1.0);
	gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;

	outNormal = mat3(objectData[gl_InstanceIndex].model) * inNormal;
	vec4 pos = objectData[gl_InstanceIndex].model * vec4(inPos, 1.0);
	outLightVec = uboScene.lightPos.xyz - pos.xyz;
	outViewVec = uboScene.viewPos.xyz - pos.xyz;
}