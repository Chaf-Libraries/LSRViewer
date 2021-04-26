#version 450

#extension GL_EXT_nonuniform_qualifier : require

layout (set = 1, binding = 0) uniform sampler2D textureArray[];

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec2 inUV;
layout (location = 4) in vec3 inViewVec;
layout (location = 5) in vec3 inLightVec;
layout (location = 6) in vec4 inTangent;
layout (location = 7) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;

layout(push_constant) uniform TextureIndexPushConsts {
	layout(offset = 64)
	uint baseColorTextureIndex;
	uint normalTextureIndex;
} textureIndex;

void main() 
{
	//texture(textures[nonuniformEXT(inTexIndex)], inUV);
	vec4 color = texture(textureArray[nonuniformEXT(textureIndex.baseColorTextureIndex)], inUV) * vec4(inColor, 1.0);

	if (ALPHA_MASK) {
		if (color.a < ALPHA_MASK_CUTOFF) {
			discard;
		}
	}

	vec3 N = inTBN * normalize(texture(textureArray[nonuniformEXT(textureIndex.normalTextureIndex)], inUV).xyz * 2.0 - vec3(1.0));

	const float ambient = 0.1;
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), ambient).rrr;
	float specular = pow(max(dot(R, V), 0.0), 32.0);
	outFragColor = vec4(diffuse * color.rgb + specular, color.a);
	// float depth=texture(depthMap, inUV).x;
	// outFragColor = vec4(vec3(depth), 1.0);


}