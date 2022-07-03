#include "scene/scene.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

namespace vme {
static vk::Filter getFilterMode(int filter_mode) {
  switch (filter_mode) {
  case TINYGLTF_TEXTURE_FILTER_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
    return vk::Filter::eNearest;
  case TINYGLTF_TEXTURE_FILTER_LINEAR:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
  case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
    return vk::Filter::eLinear;
  default:
    return vk::Filter::eLinear;
  }
}

static vk::SamplerAddressMode getSamplerAddressMode(int address_mode) {
  switch (address_mode) {
  case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
    return vk::SamplerAddressMode::eClampToEdge;
  case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
    return vk::SamplerAddressMode::eMirroredRepeat;
  case TINYGLTF_TEXTURE_WRAP_REPEAT:
    return vk::SamplerAddressMode::eRepeat;
  default:
    return vk::SamplerAddressMode::eRepeat;
  }
}

static vk::Format getFormat(int component, int pixel_type) {
  switch (component) {
  case 1:
    switch (pixel_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      return vk::Format::eR8Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return vk::Format::eR8Unorm;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      return vk::Format::eR16Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return vk::Format::eR16Unorm;
    case TINYGLTF_COMPONENT_TYPE_INT:
      return vk::Format::eR32Sint;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return vk::Format::eR32Uint;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return vk::Format::eR32Sfloat;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return vk::Format::eR64Sfloat;
    default:
      throw std::runtime_error("Unsupported pixel type");
    }
  case 2:
    switch (pixel_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      return vk::Format::eR8G8Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return vk::Format::eR8G8Unorm;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      return vk::Format::eR16G16Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return vk::Format::eR16G16Unorm;
    case TINYGLTF_COMPONENT_TYPE_INT:
      return vk::Format::eR32G32Sint;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return vk::Format::eR32G32Uint;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return vk::Format::eR32G32Sfloat;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return vk::Format::eR64G64Sfloat;
    default:
      throw std::runtime_error("Unsupported pixel type");
    }
  case 3:
    switch (pixel_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      return vk::Format::eR8G8B8Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return vk::Format::eR8G8B8Unorm;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      return vk::Format::eR16G16B16Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return vk::Format::eR16G16B16Unorm;
    case TINYGLTF_COMPONENT_TYPE_INT:
      return vk::Format::eR32G32B32Sint;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return vk::Format::eR32G32B32Uint;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return vk::Format::eR32G32B32Sfloat;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return vk::Format::eR64G64B64Sfloat;
    default:
      throw std::runtime_error("Unsupported pixel type");
    }
  case 4:
    switch (pixel_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
      return vk::Format::eR8G8B8A8Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      return vk::Format::eR8G8B8A8Unorm;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
      return vk::Format::eR16G16B16A16Snorm;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
      return vk::Format::eR16G16B16A16Unorm;
    case TINYGLTF_COMPONENT_TYPE_INT:
      return vk::Format::eR32G32B32A32Sint;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      return vk::Format::eR32G32B32A32Uint;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
      return vk::Format::eR32G32B32A32Sfloat;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
      return vk::Format::eR64G64B64A64Sfloat;
    default:
      throw std::runtime_error("Unsupported pixel type");
    }
  default:
    throw std::runtime_error("Unsupported number of components");
  }
}

Scene::Scene(gfx::Context &context, const tinygltf::Model &model) {
  // Upload buffers
  for (const auto &buffer : model.buffers) {
    auto buf = context.getAllocator().createBufferUnique(
        {{},
         buffer.data.size(),
         vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer |
             vk::BufferUsageFlagBits::eTransferDst},
        {{}, VMA_MEMORY_USAGE_AUTO});
    context.getStagingBuffer().uploadBuffer<uint8_t>(buf->getBuffer(), buffer.data,
                                                     vk::BufferCopy2{0, 0, buffer.data.size()});
    buffers_.push_back(std::move(buf));
  }
  // Upload images
  for (const auto &image : model.images) {
    const vk::Format format = getFormat(image.component, image.pixel_type);
    const vk::Extent3D extent{static_cast<uint32_t>(image.width),
                              static_cast<uint32_t>(image.height), 1};
    const vk::ImageSubresourceLayers subresource_layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    const vk::ImageSubresourceRange subresource_range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    auto img = gfx::Image(context.getAllocator(), {{},
                                                   vk::ImageType::e2D,
                                                   format,
                                                   extent,
                                                   1,
                                                   1,
                                                   vk::SampleCountFlagBits::e1,
                                                   vk::ImageTiling::eOptimal,
                                                   vk::ImageUsageFlagBits::eSampled |
                                                       vk::ImageUsageFlagBits::eTransferDst});
    img.upload<uint8_t>(context.getStagingBuffer(), vk::ImageLayout::eUndefined,
                        vk::ImageLayout::eShaderReadOnlyOptimal, subresource_range, image.image,
                        vk::BufferImageCopy2{0, 0, 0, subresource_layers, vk::Offset3D{}, extent});
    auto id = img.allocate(context.getSampledImageDescriptorHeap(),
                           {vk::ImageViewType::e2D, format, {}, subresource_range},
                           vk::ImageLayout::eShaderReadOnlyOptimal);
    images_.emplace_back(std::move(img), id);
  }
  // Upload samplers
  for (const auto &sampler : model.samplers) {
    auto samp = gfx::Sampler(context.getDevice(),
                             vk::SamplerCreateInfo{{},
                                                   getFilterMode(sampler.magFilter),
                                                   getFilterMode(sampler.minFilter),
                                                   vk::SamplerMipmapMode::eLinear,
                                                   getSamplerAddressMode(sampler.wrapS),
                                                   getSamplerAddressMode(sampler.wrapT),
                                                   getSamplerAddressMode(sampler.wrapR)});
    auto id = samp.allocate(context.getSamplerDescriptorHeap());
    samplers_.emplace_back(std::move(samp), id);
  }
  // Traverse materials
  for (const auto &material : model.materials) {
    auto getTexture = [&](int index) {
      return Texture{images_[static_cast<uint32_t>(model.textures[index].source)].second,
                     samplers_[static_cast<uint32_t>(model.textures[index].sampler)].second};
    };
    materials_.emplace_back(
        getTexture(material.pbrMetallicRoughness.baseColorTexture.index),
        getTexture(material.pbrMetallicRoughness.metallicRoughnessTexture.index),
        getTexture(material.emissiveTexture.index), getTexture(material.occlusionTexture.index),
        getTexture(material.normalTexture.index));
  }
  // Traverse scene hierarchy
  transforms_.reserve(model.meshes.size());
  meshes_.reserve(model.meshes.size());
  for (const auto &scene : model.scenes)
    for (const auto &node : model.nodes)
      addNode(model, node, glm::mat4{1.f});
}

void Scene::addNode(const tinygltf::Model &model, const tinygltf::Node &node, glm::mat4 parent) {
  glm::mat4 transform{1.f};
  if (!node.matrix.empty()) {
    for (unsigned i = 0; i < 4; ++i)
      for (unsigned j = 0; j < 4; ++j)
        transform[i][j] = static_cast<float>(node.matrix[i * 4 + j]);
  } else {
    if (!node.translation.empty())
      transform = glm::translate(transform, glm::vec3{static_cast<float>(node.translation[0]),
                                                      static_cast<float>(node.translation[1]),
                                                      static_cast<float>(node.translation[2])});
    if (!node.rotation.empty())
      transform *= glm::mat4_cast(
          glm::quat{static_cast<float>(node.rotation[0]), static_cast<float>(node.rotation[1]),
                    static_cast<float>(node.rotation[2]), static_cast<float>(node.rotation[3])});
    if (!node.scale.empty())
      transform = glm::scale(transform, glm::vec3{static_cast<float>(node.scale[0]),
                                                  static_cast<float>(node.scale[1]),
                                                  static_cast<float>(node.scale[2])});
  }
  auto child_transform = transform * parent;
  if (node.mesh != -1)
    addMesh(model, model.meshes[node.mesh], child_transform);
  for (auto child : node.children)
    addNode(model, model.nodes[child], child_transform);
}

void Scene::addMesh(const tinygltf::Model &model, const tinygltf::Mesh &mesh, glm::mat4 parent) {
  uint32_t transform_id = transforms_.size();
  transforms_.push_back(parent);
  std::vector<Primitive> primitives;
  primitives.reserve(mesh.primitives.size());
  for (const auto &primitive : mesh.primitives) {
    auto getBufferView = [&](int index) {
      const auto &buffer_view = model.bufferViews[model.accessors[index].bufferView];
      return BufferView{buffers_[buffer_view.buffer]->getBuffer(), buffer_view.byteOffset};
    };
    std::vector<BufferView> attributes;
    for (const auto &attribute : {"POSITION", "NORMAL", "TEXCOORD_0"}) {
      auto it = primitive.attributes.find(attribute);
      if (it == primitive.attributes.end())
        throw std::runtime_error("Required vertex attribute not found");
      attributes.push_back(getBufferView(it->second));
    }
    primitives.emplace_back(std::move(attributes), getBufferView(primitive.indices),
                            static_cast<uint32_t>(model.accessors[primitive.indices].count),
                            static_cast<uint32_t>(primitive.material));
  }
  meshes_.emplace_back(transform_id, std::move(primitives));
};

} // namespace vme
