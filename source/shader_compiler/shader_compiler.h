#pragma once

#include <VulkanDevice.h>
#include <VulkanTools.h>

#include <unordered_map>

namespace chaf
{
	enum class ShaderResourceType
	{
		Input,
		InputAttachment,
		Output,
		Image,
		ImageSampler,
		ImageStorage,
		Sampler,
		BufferUniform,
		BufferStorage,
		PushConstant,
		SpecializationConstant,
		All
	};

	enum class ShaderResourceMode
	{
		Static,
		Dynamic,
		UpdateAfterBind
	};

	struct ShaderResourceQualifiers
	{
		enum : uint32_t
		{
			None = 0,
			NonReadable = 1,
			NonWritable = 2,
		};
	};

	struct ShaderResource
	{
		VkShaderStageFlags stages;

		ShaderResourceType type;

		ShaderResourceMode mode;

		uint32_t set;

		uint32_t binding;

		uint32_t location;

		uint32_t input_attachment_index;

		uint32_t vec_size;

		uint32_t columns;

		uint32_t array_size;

		uint32_t offset;

		uint32_t size;

		uint32_t constant_id;

		uint32_t qualifiers;

		std::string name;
	};

	class ShaderVariant
	{
	public:
		ShaderVariant() = default;

		ShaderVariant(std::string&& preamble, std::vector<std::string>&& processes);

		void addDefinitions(const std::vector<std::string>& definitions);

		void addDefine(const std::string& def);

		void addUndefine(const std::string& undef);

		void addRuntimeArraySize(const std::string& runtime_array_name, size_t size);

		void addRuntimeArraySizes(const std::unordered_map<std::string, size_t>& sizes);

		const std::string& getPreamble() const;

		const std::vector<std::string>& getProcesses() const;

		const std::unordered_map<std::string, size_t>& getRuntimeArraySizes() const;

		void clear();

	private:
		std::string preamble;

		std::vector<std::string> processes;

		std::unordered_map<std::string, size_t> runtime_array_sizes;
	};

	class ShaderCompiler
	{
	public:
		ShaderCompiler(vks::VulkanDevice& device);

		ShaderCompiler(
			vks::VulkanDevice& device,
			VkShaderStageFlagBits stage,
			const std::string& filename,
			const std::string& entry_point,
			const ShaderVariant& shader_variant);

		bool compile();

		void setFile(const std::string& filename);

		void setStage(VkShaderStageFlagBits stage);

		void setEntryPoint(const std::string& entry_point);

		void setVariant(const ShaderVariant& variant);

		const std::vector<uint32_t>& getSpirv() const;

		const std::vector<ShaderResource>& getResources() const;

		const std::string& getInfo() const;

	private:
		std::string filename;

		vks::VulkanDevice& device;

		VkShaderStageFlagBits stage{};

		std::string entry_point;

		ShaderVariant shader_variant;

		// Compile result
		std::vector<uint32_t> spirv;

		std::vector<ShaderResource> resources;

		std::string info_log;
	};
}