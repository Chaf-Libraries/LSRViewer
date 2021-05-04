#pragma once

#include <entt.hpp>

#include <scene/components/primitive.h>
#include <scene/components/material.h>
#include <scene/components/texture.h>

#include <scene/cacher/buffer_cacher.h>

#include <VulkanTexture.h>
#include <VulkanBuffer.h>

namespace chaf
{
	class Node;

	class Scene
	{
		friend class Node;
	public:
		Scene(vks::VulkanDevice& device, const std::string& name);

		~Scene();

		Scene(const Scene&) = delete;

		Scene(Scene&&) = delete;

		Scene& operator=(const Scene&) = delete;

		Scene& operator=(Scene&&) = delete;

		Node& createNode(const std::string& name, Node& parent);

		Node& createNode(const std::string& name);

		std::vector<std::unique_ptr<Node>>& getNodes();

		std::vector<std::unique_ptr<Node>>::iterator begin();

		std::vector<std::unique_ptr<Node>>::const_iterator begin() const;

		std::vector<std::unique_ptr<Node>>::iterator end();

		std::vector<std::unique_ptr<Node>>::const_iterator end() const;

		void traverse(std::function<void(Node&)> func);

		template<typename T>
		bool hasComponent() const;

		template<typename... T, typename... Args>
		std::vector<Node*> group(Args&&... args);

		template<typename T, typename... Args>
		T& addComponent(entt::entity entity, Args&&... args);

		template<typename T, typename ...Args>
		bool hasComponent(entt::entity entity) const;

		template<typename T>
		void removeComponent(entt::entity entity);

		template<typename T>
		T& getComponent(entt::entity entity);

	private:
		vks::VulkanDevice& device;

		entt::registry registry;

		std::vector<std::unique_ptr<Node>> nodes;

		std::unordered_map<uint32_t, Node*> nodes_lookup;

		std::string name;

	public:
		struct Image {
			Texture2D texture;
		};

		struct Texture {
			int32_t imageIndex;
		};

		std::vector<Image> images;
		std::vector<Texture> textures;
		std::vector<Material> materials;
		vks::Buffer object_buffer;

		std::unique_ptr<BufferCacher> buffer_cacher;
	};
	

}

#include "scene.inl"