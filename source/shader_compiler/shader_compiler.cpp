#include <shader_compiler/shader_compiler.h>
#include <shader_compiler/glsl_compiler.h>

namespace chaf
{
	inline std::vector<uint8_t> convertToBytes(std::vector<std::string>& lines)
	{
		std::vector<uint8_t> bytes;

		for (auto& line : lines)
		{
			line += "\n";
			std::vector<uint8_t> line_bytes(line.begin(), line.end());
			bytes.insert(bytes.end(), line_bytes.begin(), line_bytes.end());
		}

		return bytes;
	}

	ShaderVariant::ShaderVariant(std::string&& preamble, std::vector<std::string>&& processes) :
		preamble{ std::move(preamble) },
		processes{ std::move(processes) }
	{
	}

	void ShaderVariant::addDefinitions(const std::vector<std::string>& definitions)
	{
		for (auto& def : definitions)
		{
			addDefine(def);
		}
	}

	void ShaderVariant::addDefine(const std::string& def)
	{
		processes.push_back("D" + def);

		std::string tmp_def = def;

		// The "=" should turn into space
		size_t pos_equal = tmp_def.find_first_of("=");
		if (pos_equal != std::string::npos)
		{
			tmp_def[pos_equal] = ' ';
		}

		preamble.append("#define " + tmp_def + "\n");
	}

	void ShaderVariant::addUndefine(const std::string& undef)
	{
		processes.push_back("U" + undef);

		preamble.append("#undef " + undef + "\n");
	}

	void ShaderVariant::addRuntimeArraySize(const std::string& runtime_array_name, size_t size)
	{
		if (runtime_array_sizes.find(runtime_array_name) == runtime_array_sizes.end())
		{
			runtime_array_sizes.insert({ runtime_array_name, size });
		}
		else
		{
			runtime_array_sizes[runtime_array_name] = size;
		}
	}

	void ShaderVariant::addRuntimeArraySizes(const std::unordered_map<std::string, size_t>& sizes)
	{
		this->runtime_array_sizes = sizes;
	}

	const std::string& ShaderVariant::getPreamble() const
	{
		return preamble;
	}

	const std::vector<std::string>& ShaderVariant::getProcesses() const
	{
		return processes;
	}

	const std::unordered_map<std::string, size_t>& ShaderVariant::getRuntimeArraySizes()
	{
		return runtime_array_sizes;
	}

	void ShaderVariant::clear()
	{
		preamble.clear();
		processes.clear();
		runtime_array_sizes.clear();
	}

	ShaderCompiler::ShaderCompiler(vks::VulkanDevice& device) :
		device{ device }
	{
	}
	ShaderCompiler::ShaderCompiler(
		vks::VulkanDevice& device,
		VkShaderStageFlagBits stage,
		const std::string& filename,
		const std::string& entry_point,
		const ShaderVariant& shader_variant) :
		device{ device },
		stage{ stage },
		filename{ filename },
		entry_point{ entry_point },
		shader_variant{ shader_variant }
	{
	}

	bool ShaderCompiler::compile()
	{
		if (entry_point.empty())
		{
			throw std::runtime_error("Shader has no specify entry point!");
		}

		std::vector<std::string> raw_data;

		std::ifstream file;

		file.open(filename, std::ios::in);

		if (!file.is_open())
		{
			throw std::runtime_error("Failed to open shader: " + filename);
		}

		auto shader_data = convertToBytes(raw_data);

		GLSLCompiler compiler;

		if (!compiler.compileToSpirv(stage, shader_data, entry_point, shader_variant, spirv, info_log))
		{
			return false;
		}

		// TODO: spirv reflection
	}

	void ShaderCompiler::setFile(const std::string& filename)
	{
		this->filename = filename;
	}

	void ShaderCompiler::setStage(VkShaderStageFlagBits stage)
	{
		this->stage = stage;
	}

	void ShaderCompiler::setEntryPoint(const std::string& entry_point)
	{
		this->entry_point = entry_point;
	}

	void ShaderCompiler::setVariant(const ShaderVariant& variant)
	{
		this->shader_variant = variant;
	}

	const std::vector<uint32_t>& ShaderCompiler::getSpirv() const
	{
		return spirv;
	}

	const std::vector<ShaderResource>& ShaderCompiler::getResources() const
	{
		return resources;
	}

	const std::string& ShaderCompiler::getInfo() const
	{
		return info_log;
	}
}