#ifndef CACHE_FACTORY_HPP
#define CACHE_FACTORY_HPP

#include <unordered_map>
#include "tracy/Tracy.hpp"

namespace vme {
template <typename Derived, typename Key, typename Value> class CacheFactory {
public:
  CacheFactory() = default;
  CacheFactory(const CacheFactory &) = delete;
  CacheFactory(CacheFactory &&) = default;
  CacheFactory &operator=(const CacheFactory &) = delete;
  CacheFactory &operator=(CacheFactory &&) = default;

  Value &get(const Key &key) {
    {
      //std::shared_lock lock(mutex_);
      if (auto it = cache_.find(key); it != cache_.end())
        return it->second;
    }
    //std::lock_guard lock(mutex_);
    return cache_.insert({key, static_cast<Derived *>(this)->create(key)}).first->second;
  }
  void reset() { cache_.clear(); }

private:
  std::unordered_map<Key, Value> cache_;
  //std::shared_mutex mutex_;
  //TracySharedLockable(std::shared_mutex, mutex_);
};
} // namespace vme

#endif
