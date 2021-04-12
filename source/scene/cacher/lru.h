#pragma once

#include <stdexcept>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace chaf
{
	class NullLock
	{
	public:
		void lock() {}
		void unlock() {}
		bool try_lock() { return true; }
	};

	class KeyNotFound :public std::invalid_argument
	{
	public:
		KeyNotFound() :std::invalid_argument("Key not found!") {}
	};

	template<typename Key_Ty, typename Val_Ty>
	struct KeyValuePair
	{
	public:
		Key_Ty key;
		Val_Ty value;

		KeyValuePair(const Key_Ty& key, Val_Ty&& value) :
			key{ key }, value{ std::move(value) }{}
	};

	template<class Key_Ty, class Val_Ty, class Lock = NullLock, class Map = std::unordered_map<Key_Ty, typename std::list<KeyValuePair<Key_Ty, std::unique_ptr<Val_Ty>>>::iterator>>
	class LruCacher
	{
	public:
		using node_type = KeyValuePair<Key_Ty, std::unique_ptr<Val_Ty>>;
		using list_type = std::list<KeyValuePair<Key_Ty, std::unique_ptr<Val_Ty>>>;
		using map_type = Map;
		using lock_type = Lock;
		using guard = std::lock_guard<lock_type>;

	public:
		LruCacher(size_t max_size = 128, size_t elastic_size = 10, std::function<void(Val_Ty&)> release_func = [](Val_Ty&) {}) :
			max_size{ max_size }, elastic_size{ elastic_size }, release{ release_func } {}

		~LruCacher()
		{
			clear();
		}

		size_t size() const
		{
			guard g(lock);
			return cache_map.size();
		}

		size_t empty() const
		{
			guard g(lock);
			return cache_map.empty();
		}

		void clear()
		{
			traverse(release);
			cache_map.clear();
			cache_list.clear();
		}

		void insert(const Key_Ty& key, std::unique_ptr<Val_Ty>&& value)
		{
			guard g(lock);

			const auto iter = cache_map.find(key);

			if (iter != cache_map.end())
			{
				LruCacher::remove(key);
				insert(key, std::move(value));
				return;
			}

			cache_list.emplace_front(key, std::move(value));
			cache_map[key] = cache_list.begin();

			prune();
		}

		bool try_get(const Key_Ty& key, Val_Ty& value)
		{
			guard g(lock);

			const auto iter = cache_map.find(key);
			if (iter == cache_map.end())
			{
				return false;
			}

			cache_list.splice(cache_list.begin(), cache_list, iter->second);
			value = *(iter->second->value);
			return true;
		}

		Val_Ty& get(const Key_Ty& key)
		{
			guard g(lock);

			const auto iter = cache_map.find(key);
			if (iter == cache_map.end())
			{
				throw KeyNotFound();
			}

			cache_list.splice(cache_list.begin(), cache_list, iter->second);
			return *(iter->second->value);
		}

		Val_Ty get_copy(const Key_Ty& key)
		{
			return get(key);
		}

		bool remove(const Key_Ty& key)
		{
			guard g(lock);

			auto iter = cache_map.find(key);
			if (iter == cache_map.end()) 
			{
				return false;
			}

			release(*(iter->second->value));
			cache_list.erase(iter->second);
			cache_map.erase(iter);
			return true;
		}

		bool contain(const Key_Ty& key) const
		{
			guard g(lock);

			return cache_map.find(key) != cache_map.end();
		}

		size_t getMaxSize() const
		{
			return max_size;
		}

		size_t getElasticSize() const
		{
			return elastic_size;
		}

		size_t getMaxAllowSize() const
		{
			return max_size + elastic_size;
		}

		void traverse(std::function<void(Val_Ty&)> func)
		{
			guard g(lock);

			for (auto& cache : cache_list)
			{
				func(*cache.value);
			}
		}

		void setRelease(std::function<void(Val_Ty&)> func)
		{
			release = func;
		}

	private:
		size_t prune()
		{
			size_t max_allow_size = max_size + elastic_size;

			if (max_size == 0 || cache_map.size() < max_allow_size)
			{
				return 0;
			}

			size_t count = 0;

			while (cache_map.size() > max_size)
			{
				cache_map.erase(cache_list.back().key);
				cache_list.pop_back();
				count++;
			}

			return count;
		}

	private:
		mutable Lock lock;

		map_type cache_map;

		list_type cache_list;

		size_t max_size;

		size_t elastic_size;

		std::function<void(Val_Ty&)> release;
	};
}