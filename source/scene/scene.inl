#pragma once

namespace chaf
{
	template<typename T>
	inline bool Scene::hasComponent() const
	{
		bool result = false;
		traverse([&result](Node& node) {
			if (node.hasComponent<T>())
			{
				result = true;
			}
			});
		return result;
	}

	template<typename ...T, typename ...Args>
	inline std::vector<Node*> Scene::group(Args && ...args)
	{
		std::vector<Node*> result;
		auto& views = registry.view<T...>(std::forward<Args>(args)...);

		for (auto& entity : views)
		{
			result.push_back(nodes_lookup.at(static_cast<uint32_t>(entity)));
		}

		return result;
	}

	template<typename T, typename ...Args>
	inline T& Scene::addComponent(entt::entity entity, Args && ...args)
	{
		if (hasComponent<T>(entity))
		{
			return getComponent<T>(entity);
		}
		return registry.emplace<T>(entity, std::forward<Args>(args)...);
	}

	template<typename T, typename ...Args>
	inline bool Scene::hasComponent(entt::entity entity) const
	{
		return registry.all_of<T, std::forward<Args>(args)...>(entity);
	}

	template<typename T>
	inline void Scene::removeComponent(entt::entity entity)
	{
		assert(hasComponent<T>(entity) && "Entity doesn't have component!");
		registry.remove<T>(entity);
	}

	template<typename T>
	inline T& Scene::getComponent(entt::entity entity)
	{
		assert(hasComponent<T>(entity) && "Entity doesn't have component!");
		return registry.get<T>(entity);
	}
}
