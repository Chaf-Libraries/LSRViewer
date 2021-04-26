#include <shader_compiler/glsl_compiler.h>

#undef max
#undef min

#include <SPIRV/GLSL.std.450.h>
#include <SPIRV/GlslangToSpv.h>
#include <StandAlone/ResourceLimits.h>
#include <glslang/Include/ShHandle.h>
#include <glslang/OSDependent/osinclude.h>

namespace chaf
{
	inline EShLanguage shaderStageConvert(VkShaderStageFlagBits stage)
	{
		switch (stage)
		{
		case VK_SHADER_STAGE_VERTEX_BIT:
			return EShLangVertex;

		case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
			return EShLangTessControl;

		case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
			return EShLangTessEvaluation;

		case VK_SHADER_STAGE_GEOMETRY_BIT:
			return EShLangGeometry;

		case VK_SHADER_STAGE_FRAGMENT_BIT:
			return EShLangFragment;

		case VK_SHADER_STAGE_COMPUTE_BIT:
			return EShLangCompute;

		case VK_SHADER_STAGE_RAYGEN_BIT_KHR:
			return EShLangRayGen;

		case VK_SHADER_STAGE_ANY_HIT_BIT_KHR:
			return EShLangAnyHit;

		case VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
			return EShLangClosestHit;

		case VK_SHADER_STAGE_MISS_BIT_KHR:
			return EShLangMiss;

		case VK_SHADER_STAGE_INTERSECTION_BIT_KHR:
			return EShLangIntersect;

		case VK_SHADER_STAGE_CALLABLE_BIT_KHR:
			return EShLangCallable;

		default:
			return EShLangVertex;
		}
	}

	glslang::EShTargetLanguage GLSLCompiler::env_target_language = glslang::EShTargetLanguage::EShTargetNone;
	glslang::EShTargetLanguageVersion GLSLCompiler::env_target_language_version = (glslang::EShTargetLanguageVersion)0;

	void GLSLCompiler::setTargetEnvironment(glslang::EShTargetLanguage target_language, glslang::EShTargetLanguageVersion target_language_version)
	{
		GLSLCompiler::env_target_language = target_language;
		GLSLCompiler::env_target_language_version = target_language_version;
	}

	void GLSLCompiler::resetTargetEnvironment()
	{
		GLSLCompiler::env_target_language = glslang::EShTargetLanguage::EShTargetNone;
		GLSLCompiler::env_target_language_version = (glslang::EShTargetLanguageVersion)0;
	}

	bool GLSLCompiler::compileToSpirv(
		VkShaderStageFlagBits stage, 
		const std::vector<uint8_t>& glsl_source, 
		const std::string& entry_point, 
		const ShaderVariant& variant, 
		std::vector<uint32_t> spirv, 
		std::string& info_log)
	{
		// Initialize glslang library.
		glslang::InitializeProcess();

		EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);

		EShLanguage language = shaderStageConvert(stage);
		std::string source = std::string(glsl_source.begin(), glsl_source.end());

		const char* file_name_list[1] = { "" };
		const char* shader_source = reinterpret_cast<const char*>(source.data());

		glslang::TShader shader(language);
		shader.setStringsWithLengthsAndNames(&shader_source, nullptr, file_name_list, 1);
		shader.setEntryPoint(entry_point.c_str());
		shader.setSourceEntryPoint(entry_point.c_str());
		shader.setPreamble(variant.getPreamble().c_str());
		shader.addProcesses(variant.getProcesses());
		if (GLSLCompiler::env_target_language != glslang::EShTargetLanguage::EShTargetNone)
		{
			shader.setEnvTarget(GLSLCompiler::env_target_language, GLSLCompiler::env_target_language_version);
		}

		if (!shader.parse(&glslang::DefaultTBuiltInResource, 100, false, messages))
		{
			info_log = std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog());
			return false;
		}

		// Add shader to new program object.
		glslang::TProgram program;
		program.addShader(&shader);

		// Link program.
		if (!program.link(messages))
		{
			info_log = std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
			return false;
		}

		// Save any info log that was generated.
		if (shader.getInfoLog())
		{
			info_log += std::string(shader.getInfoLog()) + "\n" + std::string(shader.getInfoDebugLog()) + "\n";
		}

		if (program.getInfoLog())
		{
			info_log += std::string(program.getInfoLog()) + "\n" + std::string(program.getInfoDebugLog());
		}

		glslang::TIntermediate* intermediate = program.getIntermediate(language);

		// Translate to SPIRV.
		if (!intermediate)
		{
			info_log += "Failed to get shared intermediate code.\n";
			return false;
		}

		spv::SpvBuildLogger logger;

		glslang::GlslangToSpv(*intermediate, spirv, &logger);

		info_log += logger.getAllMessages() + "\n";

		// Shutdown glslang library.
		glslang::FinalizeProcess();

		return true;
	}
}