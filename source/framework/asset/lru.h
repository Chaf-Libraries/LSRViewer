#pragma once

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <chrono>

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

	template<typename K, typename V>
	struct KeyValuePair
	{
	public:
		K key;
		V value;

		KeyValuePair(K k, V v) :key{ std::move(k) }, value{ std::move(v) }{}
	};

	template<class Key, class Value, class Lock=NullLock,class Map=std::unordered_map<Key, typename std::list<KeyValuePair<Key, Value>>::iterator>>
	class LRUCache
	{
	public:
		using node_type = KeyValuePair<Key, Value>;
		using list_type = std::list<KeyValuePair<Key, Value>>;
		using map_type = Map;
		using lock_type = Lock;
		using guard = std::lock_guard<lock_type>;

	public:
		LRUCache(size_t max_size=64, size_t expired_time = 100):
			max_size_{ max_size }, expired_time_{ expired_time }{}

		~LRUCache() = default;

		size_t size() const
		{
			guard g(lock_);
			return cache_.size();
		}

		bool empty() const
		{
			guard g(lock_);
			return cache_.empty();
		}

		void clear()
		{
			guard g(lock_);
			cache_.clear();
			keys_.clear();
		}

		void insert(const Key& key, Value& value)
		{
			guard g(lock_);
			const auto iter = cache_.find(key);
			if (iter != cache_.end())
			{
				iter->second->value = value;
				keys_.splice(keys_.begin(), keys_, iter->second);
				return;
			}

			keys_.emplace_front(key, std::move(value));
			cache_[key] = keys_.begin();
			prune();
		}

		bool tryGet(const Key& kIn, Value& vOut) 
		{
			guard g(lock_);
			const auto iter = cache_.find(kIn);
			if (iter == cache_.end()) 
			{
				return false;
			}
			keys_.splice(keys_.begin(), keys_, iter->second);
			vOut = iter->second->value;
			return true;
		}

		const Value& get(const Key& k) 
		{
			guard g(lock_);
			const auto iter = cache_.find(k);
			if (iter == cache_.end()) 
			{
				throw KeyNotFound();
			}
			keys_.splice(keys_.begin(), keys_, iter->second);
			return iter->second->value;
		}

		Value getCopy(const Key& k) 
		{
			return get(k);
		}

		bool remove(const Key& k) 
		{
			guard g(lock_);
			auto iter = cache_.find(k);
			if (iter == cache_.end()) 
			{
				return false;
			}
			keys_.erase(iter->second);
			cache_.erase(iter);
			return true;
		}

		bool contains(const Key& k) const 
		{
			guard g(lock_);
			return cache_.find(k) != cache_.end();
		}

		size_t getMaxSize() const 
		{ 
			return maxSize_; 
		}

	private:
		size_t prune()
		{
			if (max_size_ == 0 || cache_.size() < max_size_)
			{
				return 0;
			}
			size_t count = 0;
			while (cache_.size() > max_size_)
			{
				cache_.erase(keys_.back().key);
				keys_.pop_back();
				++count;
			}
			return count;
		}

	private:
		LRUCache(const LRUCache&) = delete;

		LRUCache(LRUCache&&) = delete;

		LRUCache& operator=(LRUCache&&) = delete;
		
		LRUCache& operator=(const LRUCache&) = delete;

	private:
		Lock lock_;
		map_type cache_;
		list_type keys_;
		size_t max_size_;
		size_t expired_time_;
	};
}