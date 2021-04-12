#include <scene/scene.h>
#include <scene/node.h>

namespace chaf
{
	Scene::Scene(vks::VulkanDevice& device, const std::string& name) :
		name{ name },
		device{ device }
	{
	}

	Scene::~Scene()
	{
		buffer_cacher.reset();

		for (auto& image : images)
		{
			vkDestroyImageView(device.logicalDevice, image.texture.view, nullptr);
			vkDestroyImage(device.logicalDevice, image.texture.image, nullptr);
			vkDestroySampler(device.logicalDevice, image.texture.sampler, nullptr);
			vkFreeMemory(device.logicalDevice, image.texture.deviceMemory, nullptr);
		}
		for (auto& material : materials)
		{
			vkDestroyPipeline(device.logicalDevice, material.pipeline, nullptr);
		}
	}

	Node& Scene::createNode(const std::string& name, Node& parent)
	{
		nodes.emplace_back(std::make_unique<Node>(*this, name));
		nodes_lookup.insert({ nodes.back()->getID(), &*nodes.back() });
		nodes.back()->setParent(parent);
		return *nodes.back();
	}

	Node& Scene::createNode(const std::string& name)
	{
		nodes.emplace_back(std::make_unique<Node>(*this, name));
		nodes_lookup.insert({ nodes.back()->getID(), &*nodes.back() });
		return *nodes.back();
	}

	std::vector<std::unique_ptr<Node>>& Scene::getNodes()
	{
		return nodes;
	}

	std::vector<std::unique_ptr<Node>>::iterator Scene::begin()
	{
		return nodes.begin();
	}

	std::vector<std::unique_ptr<Node>>::const_iterator Scene::begin() const
	{
		return nodes.begin();
	}

	std::vector<std::unique_ptr<Node>>::iterator Scene::end()
	{
		return nodes.end();
	}

	std::vector<std::unique_ptr<Node>>::const_iterator Scene::end() const
	{
		return nodes.end();
	}

	void Scene::traverse(std::function<void(Node&)> func)
	{
		for (auto& node : nodes)
		{
			func(*node);
		}
	}


}