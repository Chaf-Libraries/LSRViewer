#pragma once

#include <vector>

#include <tiny_gltf.h>

#include <scene/node.h>
#include <scene/scene.h>

#include <scene/components/primitive.h>

#include <VulkanDevice.h>
#include <VulkanTools.h>

// TODO: Streaming Loading

namespace chaf
{
	class SceneLoader
	{
	public:
		static std::unique_ptr<Scene> LoadFromFile(vks::VulkanDevice& device, const std::string& path, VkQueue copy_queue);

	private:
		static void parseNodes(vks::VulkanDevice& device, tinygltf::Model model, Scene& scene);
		static void parseTransform(tinygltf::Node& gltf_node, Node& node);
		static void parseCamera(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node);
		static void parseMesh(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node);
		static void parseImages(vks::VulkanDevice& device, tinygltf::Model& model, Scene& scene, VkQueue copy_queue);
		static void parseTextures(tinygltf::Model& model, Scene& scene);
		static void parseMaterials(tinygltf::Model& model, Scene& scene);

	public:
		static std::vector<uint32_t> index_buffer;
		static std::vector<Vertex> vertex_buffer;
		static std::string path;
	};
}