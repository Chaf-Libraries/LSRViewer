#version 450

#extension GL_EXT_nonuniform_qualifier : require
#extension GL_ARB_shader_draw_parameters: enable

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

layout (set = 1, binding = 0) uniform sampler2D textureArray[];

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec4 inTangent;
layout (location = 6) flat in uint inIndex;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

vec3 F_Schlick(float cosTheta, float metallic, vec3 baseColor)
{
	vec3 F0 = mix(vec3(0.04), baseColor, metallic); // * material.specular
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;    
}

vec3 BRDF(vec3 L, vec3 V, vec3 N, float metallic, float roughness, vec3 baseColor)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	// float dotNV = clamp(dot(N, V), 0.0, 1.0);
	// float dotNL = clamp(dot(N, L), 0.0, 1.0);
	// float dotLH = clamp(dot(L, H), 0.0, 1.0);
	// float dotNH = clamp(dot(N, H), 0.0, 1.0);
	float dotNV = abs(dot(N, V));
	float dotNL = abs(dot(N, L));
	float dotLH = abs(dot(L, H));
	float dotNH = abs(dot(N, H));

	// Light color fixed
	vec3 lightColor = vec3(1.0);

	vec3 color = vec3(0.0);

	if (dotNL > 0.0)
	{
		float rroughness = max(0.05, roughness);
		// D = Normal distribution (Distribution of the microfacets)
		float D = D_GGX(dotNH, roughness); 
		// G = Geometric shadowing term (Microfacets shadowing)
		float G = G_SchlicksmithGGX(dotNL, dotNV, roughness);
		// F = Fresnel factor (Reflectance depending on angle of incidence)
		vec3 F = F_Schlick(dotNV, metallic, baseColor);

		vec3 spec = abs(D * F * G / (4.0 * dotNL * dotNV));

		color += spec * dotNL * lightColor;
	}

	return color;
}

void main() 
{
	//texture(textures[nonuniformEXT(inTexIndex)], inUV);
	vec4 color = texture(textureArray[nonuniformEXT(objectData[inIndex].baseColorTextureIndex)], inUV) * vec4(inColor, 1.0);

	if (objectData[inIndex].alphaMode == 1) {
		if (color.a < objectData[inIndex].alphaCutOff) {
			discard;
		}
	}

	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN = mat3(T, B, N);

	if(objectData[inIndex].normalTextureIndex>=0)
	{
		N = TBN * normalize(texture(textureArray[nonuniformEXT(objectData[inIndex].normalTextureIndex)], inUV).xyz * 2.0 - vec3(1.0));
	}
	else
	{
		N = TBN * inNormal;
	}

	const float ambient = 0.1;
	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L), ambient).rrr;

	vec3 Lo = vec3(1.0);
	if(objectData[inIndex].metallicRoughnessTextureIndex>=0)
	{
		if(objectData[inIndex].metallicRoughnessTextureIndex == objectData[inIndex].occlusionTextureIndex)
		{
			vec4 data=texture(textureArray[nonuniformEXT(objectData[inIndex].metallicRoughnessTextureIndex)], inUV);
			Lo = BRDF(L, V, N, data.b, data.g, color.xyz);
		}
		else
		{
			Lo = BRDF(L, V, N, objectData[inIndex].metallicFactor, objectData[inIndex].roughnessFactor, color.xyz);
		}
	}
	else
	{
		Lo = diffuse * color.rgb;
		// Lo = BRDF(L, V, N, objectData[inIndex].metallicFactor, objectData[inIndex].roughnessFactor, color.xyz);
	}

	outFragColor = vec4(Lo, 1.0)+color * 0.02;
	outFragColor = pow(outFragColor, vec4(vec3(0.4545),1.0));
	//outFragColor = vec4(diffuse * color.rgb, color.a);
	//outFragColor = color;
	// float depth=texture(depthMap, inUV).x;
	//outFragColor = vec4(float(objectData[inIndex].baseColorTextureIndex)/25);


}