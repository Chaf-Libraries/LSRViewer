#pragma once

#include <entt.hpp>

namespace chaf
{
	class Scene;

	class Node
	{
	public:
		Node(Scene& scene, const std::string& name);

		Node(const Node&) = delete;

		Node(Node&&) = delete;

		Node& operator=(const Node&) = delete;

		Node& operator=(Node&&) = delete;

		void setParent(Node& node);

		void addChild(Node& node);

		const std::vector<Node*>& getChildren() const;

		Node* getParent() const;

		template<typename T, typename... Args>
		T& addComponent(Args&&... args);

		template<typename T>
		bool hasComponent() const;

		template<typename T>
		void removeComponent();

		template<typename T>
		T& getComponent();

		const uint32_t getID() const;

		const Scene& getScene() const;

		const std::string& getName() const;

	public:
		bool visibility{ true };

	private:
		std::string name;

		Scene& scene;

		entt::entity handle;

		Node* parent{ nullptr };

		std::vector<Node*> children;
	};
}

#include "node.inl"