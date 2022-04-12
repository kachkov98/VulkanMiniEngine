#ifndef COMPONENTS_HPP
#define COMPONENTS_HPP

#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <string>

struct NameComponent {
  std::string name{};
};

struct HierarchyComponent {
  entt::entity parent{entt::null};
};

struct TransformComponent {
  glm::quat rotation{};
  glm::vec3 translation{};
  float scale{1.f};
};

#endif
