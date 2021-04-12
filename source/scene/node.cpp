#include <scene/node.h>
#include <scene/scene.h>

namespace chaf
{
	Node::Node(Scene& scene, const std::string& name) :
		scene{ scene },
		name{ name },
		handle{ scene.registry.create() }
	{
	}

	void Node::setParent(Node& node)
	{
		parent = &node;
	}

	void Node::addChild(Node& node)
	{
		children.push_back(&node);
	}

	const std::vector<Node*>& Node::getChildren() const
	{
		return children;
	}

	Node* Node::getParent() const
	{
		return parent;
	}

	const uint32_t Node::getID() const
	{
		return static_cast<uint32_t>(handle);
	}

	const Scene& Node::getScene() const
	{
		return scene;
	}

	const std::string& Node::getName() const
	{
		return name;
	}
}
