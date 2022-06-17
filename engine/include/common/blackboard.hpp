#ifndef BLACKBOARD_HPP
#define BLACKBOARD_HPP

#include <any>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace vme {
class Blackboard {
public:
  template <typename T, typename... Args> void add(Args &&...args) {
    assert(!has<T>());
    storage_.emplace(typeid(T), std::any(std::in_place_type_t<T>{}, std::forward<Args>(args)....));
  }

  template <typename T> const T &get() const {
    return std::any_cast<const T &>(storage_.at(typeid(T)));
  }
  template <typename T> T &get() {
    return const_cast<T &>(const_cast<const Blacboard *>(this)->get<T>());
  }

  template <typename T> bool has() const { return storage_.contains(typeid(T)); }
  void reset() { storage_.clear(); }

private:
  std::unordered_map<std::type_index, std::any> storage_;
};
} // namespace vme

#endif
