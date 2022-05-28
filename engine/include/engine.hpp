#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <entt/locator/locator.hpp>

namespace vme {
class Engine {
public:
  static void init();
  static void terminate();
  template <typename Service> static Service &get() {
    return entt::locator<Service>::value();
  }
};
} // namespace vme

#endif
