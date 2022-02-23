#include "descriptors.hpp"
#include <array>

namespace gfx {
static constexpr std::array descriptor_sizes = std::to_array<std::pair<vk::DescriptorType, float>>(
    {{vk::DescriptorType::eSampler, 0.5f},
     {vk::DescriptorType::eCombinedImageSampler, 4.f},
     {vk::DescriptorType::eSampledImage, 4.f},
     {vk::DescriptorType::eStorageImage, 1.f},
     {vk::DescriptorType::eUniformTexelBuffer, 1.f},
     {vk::DescriptorType::eStorageTexelBuffer, 1.f},
     {vk::DescriptorType::eUniformBuffer, 2.f},
     {vk::DescriptorType::eStorageBuffer, 2.f},
     {vk::DescriptorType::eUniformBufferDynamic, 1.f},
     {vk::DescriptorType::eStorageBufferDynamic, 1.f},
     {vk::DescriptorType::eInputAttachment, 0.5f}});

vk::DescriptorPool DescriptorAllocator::getPool(unsigned size) {
  if (free_pools_.empty()) {
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(descriptor_sizes.size());
    for (const auto &desc : descriptor_sizes)
      pool_sizes.emplace_back(desc.first, size * desc.second);
    used_pools_.push_back(device_.createDescriptorPoolUnique({{}, size, pool_sizes}));
  } else {
    auto pool = std::move(free_pools_.back());
    free_pools_.pop_back();
    used_pools_.push_back(std::move(pool));
  }
  return *used_pools_.back();
}

vk::UniqueDescriptorSet
DescriptorAllocator::allocate(const vk::DescriptorSetLayout &descriptor_layout) {
  vk::DescriptorSet descriptor_set;
  if (!current_pool_)
    current_pool_ = getPool();
  vk::DescriptorSetAllocateInfo alloc_info{current_pool_, descriptor_layout};
  auto result = device_.allocateDescriptorSets(&alloc_info, &descriptor_set);
  if (result == vk::Result::eErrorFragmentedPool || result == vk::Result::eErrorOutOfPoolMemory) {
    alloc_info.descriptorPool = current_pool_ = getPool();
    result = device_.allocateDescriptorSets(&alloc_info, &descriptor_set);
  }
  auto deleter = vk::PoolFree(device_, current_pool_, VULKAN_HPP_DEFAULT_DISPATCHER);
  return vk::UniqueDescriptorSet(descriptor_set, deleter);
}

void DescriptorAllocator::reset() {
  for (const auto &pool : used_pools_)
    device_.resetDescriptorPool(*pool);
  free_pools_.insert(free_pools_.end(), std::make_move_iterator(used_pools_.begin()),
                     std::make_move_iterator(used_pools_.end()));
  used_pools_.clear();
  current_pool_ = nullptr;
}

vk::DescriptorSetLayout
DescriptorLayoutCache::getDescriptorLayout(const vk::DescriptorSetLayoutCreateInfo &layout_info) {
  if (auto it = cache_.find(layout_info); it != cache_.end())
    return *it->second;
  auto &layout = cache_[layout_info] = device_.createDescriptorSetLayoutUnique(layout_info);
  return *layout;
}

void DescriptorLayoutCache::reset() { cache_.clear(); }

DescriptorBuilder &
DescriptorBuilder::bindBuffers(uint32_t binding,
                               vk::ArrayProxyNoTemporaries<vk::DescriptorBufferInfo> buffer_info,
                               vk::DescriptorType type, vk::ShaderStageFlags stage_flags) {
  bindings_.emplace_back(binding, type, buffer_info.size(), stage_flags, nullptr);
  writes_.emplace_back();
  return *this;
}

DescriptorBuilder &
DescriptorBuilder::bindImages(uint32_t binding,
                              vk::ArrayProxyNoTemporaries<vk::DescriptorImageInfo> image_info,
                              vk::DescriptorType type, vk::ShaderStageFlags stage_flags) {
  bindings_.emplace_back(binding, type, image_info.size(), stage_flags, nullptr);
  return *this;
}

std::pair<vk::UniqueDescriptorSet, vk::DescriptorSetLayout> DescriptorBuilder::build() {
  auto layout = descriptor_layout_cache_->getDescriptorLayout({{}, bindings_});
  auto descriptor = descriptor_allocator_->allocate(layout);

  return {std::move(descriptor), layout};
}
} // namespace gfx