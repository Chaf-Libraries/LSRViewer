#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_EXTERNAL_IMAGE

#include <scene/scene_loader.h>
#include <scene/components/transform.h>
#include <scene/components/camera.h>
#include <scene/components/mesh.h>

#include <filesystem>
#include <iostream>

#include <glm/gtc/type_ptr.hpp>

#include <vulkan/vulkan.h>

namespace chaf
{
	std::vector<uint32_t> SceneLoader::index_buffer;
	std::vector<Vertex> SceneLoader::vertex_buffer;
	std::string SceneLoader::path = "";

	std::unique_ptr<Scene> SceneLoader::LoadFromFile(vks::VulkanDevice& device, const std::string& path, VkQueue copy_queue)
	{
		size_t pos = path.find_last_of('/');
		SceneLoader::path = path.substr(0, pos);

		index_buffer.clear();
		vertex_buffer.clear();

		tinygltf::Model gltf_input;
		tinygltf::TinyGLTF gltf_context;
		std::string error, warning;
		bool file_loaded{ false };

		if (std::filesystem::path::path(path).extension().string() == ".gltf")
		{
			file_loaded= gltf_context.LoadASCIIFromFile(&gltf_input, &error, &warning, path);
		}
		else if (std::filesystem::path::path(path).extension().string() == ".glb")
		{
			file_loaded = gltf_context.LoadBinaryFromFile(&gltf_input, &error, &warning, path);
		}
		
		if(!file_loaded)
		{
			std::cout << "Invalid scene file!" << std::endl;
			return nullptr;
		}

		auto scene = std::make_unique<Scene>(device, "Scene");

		// Parse nodes
		parseNodes(device, gltf_input, *scene);
		parseImages(device, gltf_input, *scene, copy_queue);
		parseTextures(gltf_input, *scene);
		parseMaterials(gltf_input, *scene);

		return scene;
	}

	void SceneLoader::parseNodes(vks::VulkanDevice& device, tinygltf::Model model, Scene& scene)
	{
		// Add all nodes
		for (auto gltf_node : model.nodes)
		{
			auto& node = scene.createNode(gltf_node.name);
			parseTransform(gltf_node, node);
			parseCamera(model, gltf_node, node);
			parseMesh(model, gltf_node, node);
		}

		auto& nodes = scene.getNodes();

		// Deal with relationship
		auto& node_it = nodes.begin();
		for (auto gltf_node : model.nodes)
		{
			for (auto child_index : gltf_node.children)
			{
				(*node_it)->addChild(*nodes[child_index]);
				nodes[child_index]->setParent(*(*node_it));
			}
			node_it++;
		}
	}

	void SceneLoader::parseTransform(tinygltf::Node& gltf_node, Node& node)
	{
		auto& transform = node.addComponent<Transform>(&node);
		
		if (gltf_node.translation.size() > 0)
		{
			transform.setTranslation(glm::vec3(glm::make_vec3(gltf_node.translation.data())));
		}

		if (gltf_node.rotation.size() > 0)
		{
			transform.setRotation(glm::quat(glm::make_quat(gltf_node.rotation.data())));
		}

		if (gltf_node.scale.size() > 0)
		{
			transform.setScale(glm::vec3(glm::make_vec3(gltf_node.scale.data())));
		}

		if (gltf_node.matrix.size() > 0)
		{
			transform.setMatrix(glm::mat4(glm::make_mat4(gltf_node.matrix.data())));
		}
	}

	void SceneLoader::parseCamera(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node)
	{
		if (gltf_node.camera == -1)
		{
			return;
		}

		auto& gltf_camera = model.cameras[gltf_node.camera];
		auto& camera = node.addComponent<Camera>(&node);

		if (gltf_camera.type == "perspective")
		{
			camera.setType(CameraType::Perspective);
			camera.setAspectRatio(static_cast<float>(gltf_camera.perspective.aspectRatio));
			camera.setFieldOfView(static_cast<float>(gltf_camera.perspective.yfov));
			camera.setNearPlane(static_cast<float>(gltf_camera.perspective.znear));
			camera.setFarPlane(static_cast<float>(gltf_camera.perspective.zfar));
		}
		else if (gltf_camera.type == "orthographic")
		{
			camera.setType(CameraType::Orthographic);
			camera.setTop(static_cast<float>(gltf_camera.orthographic.ymag));
			camera.setBottom(-static_cast<float>(gltf_camera.orthographic.ymag));
			camera.setLeft(-static_cast<float>(gltf_camera.orthographic.xmag));
			camera.setRight(static_cast<float>(gltf_camera.orthographic.xmag));
			camera.setNearPlane(static_cast<float>(gltf_camera.perspective.znear));
			camera.setFarPlane(static_cast<float>(gltf_camera.perspective.zfar));
		}
		else
		{
			std::cout << "Unknow camera type!" << std::endl;
			return;
		}
	}

	void SceneLoader::parseMesh(tinygltf::Model& model, tinygltf::Node& gltf_node, Node& node)
	{
		if (gltf_node.mesh == -1)
		{
			return;
		}

		auto& gltf_mesh = model.meshes[gltf_node.mesh];
		auto& mesh = node.addComponent<Mesh>();

		for (auto gltf_primitive : gltf_mesh.primitives)
		{
			// empty primitive
			Primitive primitive;

			// start point
			uint32_t first_index = static_cast<uint32_t>(index_buffer.size());
			uint32_t vertex_start = static_cast<uint32_t>(vertex_buffer.size());
			uint32_t index_count = 0;

			// buffer data
			size_t vertex_count = 0;

			// Vertex
			{
				const float* position_buffer = nullptr;
				const float* normals_buffer = nullptr;
				const float* texCoords_buffer = nullptr;
				const float* tangents_buffer = nullptr;

				// Check position
				if (gltf_primitive.attributes.find("POSITION") != gltf_primitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[gltf_primitive.attributes.find("POSITION")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					position_buffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
					vertex_count = accessor.count;
					// Check bounding box
					if (accessor.maxValues.size() == 3)
					{
						primitive.bbox.update(glm::vec3(glm::make_vec3(accessor.maxValues.data())));
					}
					if (accessor.minValues.size() == 3)
					{
						primitive.bbox.update(glm::vec3(glm::make_vec3(accessor.minValues.data())));
					}
				}

				// Check normal
				if (gltf_primitive.attributes.find("NORMAL") != gltf_primitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[gltf_primitive.attributes.find("NORMAL")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					normals_buffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Check uv
				if (gltf_primitive.attributes.find("TEXCOORD_0") != gltf_primitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[gltf_primitive.attributes.find("TEXCOORD_0")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					texCoords_buffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// Check tangent
				if (gltf_primitive.attributes.find("TANGENT") != gltf_primitive.attributes.end()) {
					const tinygltf::Accessor& accessor = model.accessors[gltf_primitive.attributes.find("TANGENT")->second];
					const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
					tangents_buffer = reinterpret_cast<const float*>(&(model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
				}

				// append vertex buffer
				for (size_t v = 0; v < vertex_count; v++) {
					Vertex vert{};
					vert.pos = glm::vec4(glm::make_vec3(&position_buffer[v * 3]), 1.0f);
					vert.normal = glm::normalize(glm::vec3(normals_buffer ? glm::make_vec3(&normals_buffer[v * 3]) : glm::vec3(0.0f)));
					vert.uv = texCoords_buffer ? glm::make_vec2(&texCoords_buffer[v * 2]) : glm::vec3(0.0f);
					vert.color = glm::vec3(1.0f);
					vert.tangent = tangents_buffer ? glm::make_vec4(&tangents_buffer[v * 4]) : glm::vec4(0.0f);
					vertex_buffer.push_back(vert);
				}
			}

			// Indices
			{
				const tinygltf::Accessor& accessor = model.accessors[gltf_primitive.indices];
				const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
				const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

				index_count += static_cast<uint32_t>(accessor.count);

				// glTF supports different component types of indices
				switch (accessor.componentType) {
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
					uint32_t* buf = new uint32_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
					for (size_t index = 0; index < accessor.count; index++) {
						index_buffer.push_back(buf[index] + vertex_start);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
					uint16_t* buf = new uint16_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
					for (size_t index = 0; index < accessor.count; index++) {
						index_buffer.push_back(buf[index] + vertex_start);
					}
					break;
				}
				case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
					uint8_t* buf = new uint8_t[accessor.count];
					memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
					for (size_t index = 0; index < accessor.count; index++) {
						index_buffer.push_back(buf[index] + vertex_start);
					}
					break;
				}
				default:
					std::cerr << "Index component type " << accessor.componentType << " not supported!" << std::endl;
					return;
				}
			}

			primitive.first_index = first_index;
			primitive.index_count = index_count;
			primitive.material_index = gltf_primitive.material;

			mesh.addPrimitive(primitive);
		}
	}

	void SceneLoader::parseImages(vks::VulkanDevice& device, tinygltf::Model& model, Scene& scene, VkQueue copy_queue)
	{
		scene.images.resize(model.images.size());
		for (size_t i = 0; i < model.images.size(); i++) {
			tinygltf::Image& glTFImage = model.images[i];
			scene.images[i].texture.loadFromFile(path + "/" + glTFImage.uri, VK_FORMAT_R8G8B8A8_UNORM, &device, copy_queue);
		}
	}

	void SceneLoader::parseTextures(tinygltf::Model& model, Scene& scene)
	{
		scene.textures.resize(model.textures.size());
		for (size_t i = 0; i < model.textures.size(); i++) 
		{
			scene.textures[i].imageIndex = model.textures[i].source;
		}
	}

	void SceneLoader::parseMaterials(tinygltf::Model& model, Scene& scene)
	{
		scene.materials.resize(model.materials.size());
		for (size_t i = 0; i < model.materials.size(); i++) {
			// We only read the most basic properties required for our sample
			tinygltf::Material glTFMaterial = model.materials[i];
			// Get the base color factor
			if (glTFMaterial.values.find("baseColorFactor") != glTFMaterial.values.end()) {
				scene.materials[i].baseColorFactor = glm::make_vec4(glTFMaterial.values["baseColorFactor"].ColorFactor().data());
			}
			// Get base color texture index
			if (glTFMaterial.values.find("baseColorTexture") != glTFMaterial.values.end()) {
				scene.materials[i].baseColorTextureIndex = glTFMaterial.values["baseColorTexture"].TextureIndex();
			}
			// Get the normal map texture index
			if (glTFMaterial.additionalValues.find("normalTexture") != glTFMaterial.additionalValues.end()) {
				scene.materials[i].normalTextureIndex = glTFMaterial.additionalValues["normalTexture"].TextureIndex();
			}
			// Get some additional material parameters that are used in this sample
			scene.materials[i].alphaMode = glTFMaterial.alphaMode;
			scene.materials[i].alphaCutOff = (float)glTFMaterial.alphaCutoff;
			scene.materials[i].doubleSided = glTFMaterial.doubleSided;
		}
	}
}