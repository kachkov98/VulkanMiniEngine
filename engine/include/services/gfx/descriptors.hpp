#ifndef DESCRIPTORS_HPP
#define DESCRIPTORS_HPP

#include "common/cache.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <algorithm>
#include <vector>

namespace gfx {

class DescriptorSetLayoutCache final
    : public vme::Cache<DescriptorSetLayoutCache, vk::DescriptorSetLayoutCreateInfo,
                        vk::UniqueDescriptorSetLayout> {
public:
  DescriptorSetLayoutCache(vk::Device device = {}) : device_(device) {}

  vk::UniqueDescriptorSetLayout create(const vk::DescriptorSetLayoutCreateInfo &create_info) {
    return device_.createDescriptorSetLayoutUnique(create_info);
  }

private:
  vk::Device device_;
};

class DescriptorSetLayoutBuilder {
public:
  DescriptorSetLayoutBuilder(DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : descriptor_set_layout_cache_(&descriptor_set_layout_cache) {}

  DescriptorSetLayoutBuilder &binding(uint32_t binding, vk::DescriptorType type, uint32_t count,
                                      vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll) {
    bindings_.emplace_back(binding, type, count, stages);
    return *this;
  }

  DescriptorSetLayoutBuilder &
  binding(uint32_t binding, vk::DescriptorType type,
          const vk::ArrayProxyNoTemporaries<const vk::Sampler> &immutable_samplers,
          vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eAll) {
    assert(type == vk::DescriptorType::eSampler ||
           type == vk::DescriptorType::eCombinedImageSampler);
    bindings_.emplace_back(binding, type, stages, immutable_samplers);
    return *this;
  }

  vk::DescriptorSetLayout build() {
    std::sort(bindings_.begin(), bindings_.end());
    return *descriptor_set_layout_cache_->get({{}, bindings_});
  }

private:
  DescriptorSetLayoutCache *descriptor_set_layout_cache_{nullptr};

  std::vector<vk::DescriptorSetLayoutBinding> bindings_;
};

class DescriptorSetAllocator final {
public:
  DescriptorSetAllocator() = default;
  DescriptorSetAllocator(vk::Device device) noexcept : device_(device) {}

  vk::UniqueDescriptorSet allocate(const vk::DescriptorSetLayout &descriptor_layout,
                                   const vk::ArrayProxy<const vk::WriteDescriptorSet> &bindings);
  void reset();

private:
  vk::Device device_{};
  vk::DescriptorPool current_pool_{};
  std::vector<vk::UniqueDescriptorPool> used_pools_;
  std::vector<vk::UniqueDescriptorPool> free_pools_;

  vk::DescriptorPool getPool(unsigned size = 1024);
};

class DescriptorSet final {
public:
  DescriptorSet() = default;
  DescriptorSet(vk::UniqueDescriptorSet &&descriptor_set, vk::DescriptorSetLayout layout)
      : descriptor_set_(std::move(descriptor_set)), layout_(layout) {}

  vk::DescriptorSet get() const noexcept { return *descriptor_set_; }
  vk::DescriptorSetLayout getLayout() const noexcept { return layout_; }
  void reset() noexcept {
    descriptor_set_.reset();
    layout_ = nullptr;
  }

private:
  vk::UniqueDescriptorSet descriptor_set_ = {};
  vk::DescriptorSetLayout layout_ = {};
};

class DescriptorSetBuilder final : public DescriptorSetLayoutBuilder {
public:
  DescriptorSetBuilder(DescriptorSetAllocator &descriptor_allocator,
                       DescriptorSetLayoutCache &descriptor_layout_cache)
      : DescriptorSetLayoutBuilder(descriptor_layout_cache),
        descriptor_allocator_(&descriptor_allocator){};

  DescriptorSetBuilder &
  bind(uint32_t binding, vk::DescriptorType type,
       const vk::ArrayProxyNoTemporaries<const vk::DescriptorBufferInfo> &buffer_info,
       vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll) {
    {
      DescriptorSetLayoutBuilder::binding(binding, type, buffer_info.size(), stage_flags);
      bindings_.emplace_back(nullptr, binding, 0, type, nullptr, buffer_info);
      return *this;
    }
  }
  DescriptorSetBuilder &
  bind(uint32_t binding, vk::DescriptorType type,
       const vk::ArrayProxyNoTemporaries<const vk::DescriptorImageInfo> &image_info,
       const vk::ArrayProxyNoTemporaries<const vk::Sampler> &immutable_samplers = {},
       vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll) {
    assert(immutable_samplers.empty() || image_info.size() == immutable_samplers.size());
    DescriptorSetLayoutBuilder::binding(binding, type, immutable_samplers, stage_flags);
    bindings_.emplace_back(nullptr, binding, 0, type, image_info, nullptr);
    return *this;
  }

  DescriptorSet build();

private:
  DescriptorSetAllocator *descriptor_allocator_{nullptr};

  std::vector<vk::WriteDescriptorSet> bindings_;
};
} // namespace gfx

#endif
