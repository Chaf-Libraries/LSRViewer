#include <resource/resource_manager.h>

#include <framework/gltf_loader.h>

namespace chaf
{
	ResourceManager::ResourceManager(vkb::Device& device) :
		device{ device }
	{
		if (thread_pool.size() < 1)
		{
			thread_pool.resize(1);
		}
	}

	void ResourceManager::update(std::function<void(std::unique_ptr<vkb::sg::Scene>&)> callback)
	{
		if (is_dirty)
		{
			callback(scene_cache);
			is_dirty = false;
			scene_cache = nullptr;
		}
	}

	void ResourceManager::require_new_scene(const std::string& filepath)
	{
		if (!is_loading)
		{
			is_loading = true;
			thread_pool.push([this, filepath](size_t thread_id)
				{ 
					try
					{
						this->scene_cache = this->load_scene(filepath, 1);
						this->is_dirty = true;
						this->is_loading = false;
					}
					catch (std::runtime_error& e)
					{
						LOGW(e.what());
					}
				});
		}
	}

	void ResourceManager::require_new_model(const std::string& filepath)
	{


	}

	std::unique_ptr<vkb::sg::Scene> ResourceManager::load_scene(const std::string& path, int scene_index)
	{
		vkb::GLTFLoader loader{ device };

		auto result = loader.read_scene_from_file(path, scene_index);

		if (!result)
		{
			LOGE("Cannot load scene: {}", path.c_str());
			throw std::runtime_error("Cannot load scene: " + path);
		}

		return result;
	}
}