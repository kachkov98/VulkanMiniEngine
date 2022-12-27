#pragma once

#include <glm/gtc/type_ptr.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#define IM3D_VEC2_APP                                                                              \
  constexpr Vec2(const glm::vec2 &v) : x(v.x), y(v.y) {}                                           \
  operator glm::vec2() const { return glm::vec2(x, y); }
#define IM3D_VEC3_APP                                                                              \
  constexpr Vec3(const glm::vec3 &v) : x(v.x), y(v.y), z(v.z) {}                                   \
  operator glm::vec3() const { return glm::vec3(x, y, z); }
#define IM3D_VEC4_APP                                                                              \
  constexpr Vec4(const glm::vec4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}                           \
  operator glm::vec4() const { return glm::vec4(x, y, z, w); }
#define IM3D_MAT3_APP                                                                              \
  constexpr Mat3(const glm::mat3 &m)                                                               \
      : m{m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2], m[2][0], m[2][1], m[2][2]} {}      \
  operator glm::mat3() const { return glm::make_mat3(m); }
#define IM3D_MAT4_APP                                                                              \
  constexpr Mat4(const glm::mat4 &m)                                                               \
      : m{m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3],                  \
          m[2][0], m[2][1], m[2][2], m[2][3], m[3][0], m[3][1], m[3][2], m[3][3]} {}               \
  operator glm::mat4() const { return glm::make_mat4(m); }
