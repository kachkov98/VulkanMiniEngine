#ifndef SCENE_HPP
#define SCENE_HPP

#include <glm/mat4x4.hpp>

#include "services/gfx/context.hpp"

namespace tinygltf {
class Model;
class Node;
class Mesh;
} // namespace tinygltf

namespace vme {
class Scene {
public:
  struct Texture {
    uint32_t texture_id, sampler_id;
  };
  struct Material {
    Texture albedo, metallic_roughness, emmisive, ao, normal;
  };

  struct BufferView {
    vk::Buffer buffer;
    vk::DeviceSize offset;
  };
  struct Primitive {
    std::vector<BufferView> attributes;
    BufferView indices;
    uint32_t count;
    uint32_t material_id;
  };
  struct Mesh {
    uint32_t transform_id;
    std::vector<Primitive> primitives;
  };

  Scene() = default;
  Scene(gfx::Context &context, const tinygltf::Model &model);

  const std::vector<glm::mat4> getTransforms() const noexcept { return transforms_; }
  const std::vector<Material> getMaterials() const noexcept { return materials_; }
  const std::vector<Mesh> &getMeshes() const noexcept { return meshes_; }

private:
  std::vector<vma::UniqueBuffer> buffers_;
  std::vector<std::pair<gfx::Image, uint32_t>> images_;
  std::vector<std::pair<gfx::Sampler, uint32_t>> samplers_;

  std::vector<glm::mat4> transforms_;
  std::vector<Material> materials_;

  std::vector<Mesh> meshes_;

  void addNode(const tinygltf::Model &model, const tinygltf::Node &node, glm::mat4 parent);
  void addMesh(const tinygltf::Model &model, const tinygltf::Mesh &mesh, glm::mat4 parent);
};
} // namespace vme
#endif
