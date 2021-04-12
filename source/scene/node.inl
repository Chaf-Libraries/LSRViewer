#pragma once

namespace chaf
{
	template<typename T, typename... Args>
	T& Node::addComponent(Args&&... args)
	{
		return scene.addComponent<T>(handle, std::forward<Args>(args)...);
	}

	template<typename T>
	bool Node::hasComponent() const
	{
		return scene.hasComponent<T>(handle);
	}

	template<typename T>
	void Node::removeComponent()
	{
		scene.removeComponent<T>(handle);
	}

	template<typename T>
	T& Node::getComponent()
	{
		return scene.getComponent<T>(handle);
	}
}