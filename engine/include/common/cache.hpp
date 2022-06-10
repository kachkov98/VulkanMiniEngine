#ifndef CACHE_HPP
#define CACHE_HPP

#include <unordered_map>

namespace vme {
template <typename Derived, typename Key, typename Value> class Cache {
public:
  Cache() = default;
  Cache(const Cache &) = delete;
  Cache(Cache &&) = default;
  Cache &operator=(const Cache &) = delete;
  Cache &operator=(Cache &&) = default;

  Value &get(const Key &key) {
    if (auto it = cache_.find(key); it != cache_.end())
      return it->second;
    return cache_.insert({key, static_cast<Derived *>(this)->create(key)}).first->second;
  }
  void reset() { cache_.clear(); }

private:
  std::unordered_map<Key, Value> cache_;
};
} // namespace vme

#endif
