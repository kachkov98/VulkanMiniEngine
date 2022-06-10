#ifndef DESCRIPTORS_HPP
#define DESCRIPTORS_HPP

#include "common/cache.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

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

class DescriptorSetAllocator final {
public:
  DescriptorSetAllocator() = default;
  DescriptorSetAllocator(vk::Device device) noexcept : device_(device) {}

  vk::UniqueDescriptorSet allocate(const vk::DescriptorSetLayout &descriptor_layout);
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

class DescriptorSetBuilder final {
public:
  DescriptorSetBuilder(DescriptorSetAllocator &descriptor_allocator,
                       DescriptorSetLayoutCache &descriptor_layout_cache)
      : descriptor_allocator_(&descriptor_allocator),
        descriptor_set_layout_cache_(&descriptor_layout_cache){};

  DescriptorSetBuilder &bind(uint32_t binding,
                             vk::ArrayProxyNoTemporaries<vk::DescriptorBufferInfo> buffer_info,
                             vk::DescriptorType type,
                             vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll) {
    {
      bindings_.emplace_back(binding, type, buffer_info.size(), stage_flags, nullptr);
      writes_.emplace_back();
      return *this;
    }
  }
  DescriptorSetBuilder &bind(uint32_t binding,
                             vk::ArrayProxyNoTemporaries<vk::DescriptorImageInfo> image_info,
                             vk::DescriptorType type,
                             vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll) {
    bindings_.emplace_back(binding, type, image_info.size(), stage_flags, nullptr);
    return *this;
  }

  DescriptorSet build() {
    auto &layout = *descriptor_set_layout_cache_->get({{}, bindings_});
    return DescriptorSet(descriptor_allocator_->allocate(layout), layout);
  }

private:
  DescriptorSetAllocator *descriptor_allocator_{nullptr};
  DescriptorSetLayoutCache *descriptor_set_layout_cache_{nullptr};

  std::vector<vk::DescriptorSetLayoutBinding> bindings_;
  std::vector<vk::WriteDescriptorSet> writes_;
};
} // namespace gfx

#endif
