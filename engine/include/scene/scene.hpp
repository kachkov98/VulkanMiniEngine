#ifndef SCENE_HPP
#define SCENE_HPP

#include <entt/entity/registry.hpp>

namespace vme {
class Scene {
public:
  void addEntity(entt::entity parent = entt::null);

private:
  entt::registry registry_;
};
} // namespace vme
#endif
