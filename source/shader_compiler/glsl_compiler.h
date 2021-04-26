#pragma once

#include <glslang/Public/ShaderLang.h>

#include <shader_compiler/shader_compiler.h>

namespace chaf
{
	class GLSLCompiler
	{
	public:
		static void setTargetEnvironment(glslang::EShTargetLanguage target_language, glslang::EShTargetLanguageVersion target_language_version);

		static void resetTargetEnvironment();

		bool compileToSpirv(
			VkShaderStageFlagBits stage,
			const std::vector<uint8_t>& glsl_source,
			const std::string& entry_point,
			const ShaderVariant& variant,
			std::vector<uint32_t> spirv,
			std::string& info_log);

	private:
		static glslang::EShTargetLanguage env_target_language;
		static glslang::EShTargetLanguageVersion env_target_language_version;
	};
}