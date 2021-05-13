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
		static std::unique_ptr<Scene> LoadFromFile(vks::VulkanDevice& device, const std::string& path, VkQueue& copy_queue);


	private:
		static void parseNodes(vks::VulkanDevice& device, tinygltf::Model model, Scene& scene);
		static void parseTransform(tinygltf::Node& gltf_node, Node& node);
		static void parseCamera(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node);
		static void parsePrimitives(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node, Scene& scene);
		static void parseImages(vks::VulkanDevice& device, tinygltf::Model& model, Scene& scene, VkQueue copy_queue);
		static void parseTextures(tinygltf::Model& model, Scene& scene);
		static void parseMesh(tinygltf::Model& model, Scene& scene);
		static void parseMaterials(tinygltf::Model& model, Scene& scene);
		static void parseExtensions(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node);
		static void parseLight(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node);

		static void genPrimitiveBuffer(vks::VulkanDevice& device, Scene& scene, VkQueue& queue);

	public:
		static std::vector<uint32_t> index_buffer;
		static std::vector<Vertex> vertex_buffer;
		static std::vector<std::vector<Primitive>> primitives;
		static std::string path;
	};
}