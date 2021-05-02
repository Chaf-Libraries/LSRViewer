#pragma once

#include <shader_compiler/shader_compiler.h>

#include <spirv_glsl.hpp>

namespace chaf
{
	class SpirvReflection
	{
	public:
		static bool reflectShaderResources(
			VkShaderStageFlagBits stage,
			const std::vector<uint32_t>& spirv,
			std::vector<ShaderResource>& resources,
			const ShaderVariant& variant);

	private:
		static void parseShaderResources(
			const spirv_cross::Compiler& compiler,
			VkShaderStageFlagBits stage,
			std::vector<ShaderResource>& resources,
			const ShaderVariant& variant);

		static void parsePushConstants(
			const spirv_cross::Compiler& compiler,
			VkShaderStageFlagBits stage,
			std::vector<ShaderResource>& resources,
			const ShaderVariant& variant);

		static void parseSpecializationConstants(
			const spirv_cross::Compiler& compiler,
			VkShaderStageFlagBits stage,
			std::vector<ShaderResource>& resources,
			const ShaderVariant& variant);
	};
}