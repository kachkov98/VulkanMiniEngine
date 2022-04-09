#ifndef DESCRIPTORS_HPP
#define DESCRIPTORS_HPP

#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

namespace gfx {

class DescriptorAllocator final {
public:
  DescriptorAllocator() = default;
  DescriptorAllocator(vk::Device device) noexcept : device_(device) {}

  vk::UniqueDescriptorSet allocate(const vk::DescriptorSetLayout &descriptor_layout);
  void reset();

private:
  vk::Device device_{};
  vk::DescriptorPool current_pool_{};
  std::vector<vk::UniqueDescriptorPool> used_pools_;
  std::vector<vk::UniqueDescriptorPool> free_pools_;

  vk::DescriptorPool getPool(unsigned size = 1024);
};

class DescriptorLayoutCache final {
public:
  DescriptorLayoutCache() = default;
  DescriptorLayoutCache(vk::Device device) : device_(device) {}

  vk::DescriptorSetLayout getDescriptorLayout(const vk::DescriptorSetLayoutCreateInfo &layout_info);
  void reset();

private:
  vk::Device device_;
  std::unordered_map<vk::DescriptorSetLayoutCreateInfo, vk::UniqueDescriptorSetLayout> cache_;
};

class DescriptorBuilder final {
public:
  DescriptorBuilder(DescriptorAllocator &descriptor_allocator,
                    DescriptorLayoutCache &descriptor_layout_cache) noexcept
      : descriptor_allocator_(&descriptor_allocator),
        descriptor_layout_cache_(&descriptor_layout_cache){};

  DescriptorBuilder &bindBuffers(uint32_t binding,
                                 vk::ArrayProxyNoTemporaries<vk::DescriptorBufferInfo> buffer_info,
                                 vk::DescriptorType type, vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll);
  DescriptorBuilder &bindImages(uint32_t binding,
                                vk::ArrayProxyNoTemporaries<vk::DescriptorImageInfo> image_info,
                                vk::DescriptorType type, vk::ShaderStageFlags stage_flags = vk::ShaderStageFlagBits::eAll);

  std::pair<vk::UniqueDescriptorSet, vk::DescriptorSetLayout> build();

private:
  DescriptorAllocator *descriptor_allocator_{nullptr};
  DescriptorLayoutCache *descriptor_layout_cache_{nullptr};

  std::vector<vk::DescriptorSetLayoutBinding> bindings_;
  std::vector<vk::WriteDescriptorSet> writes_;
};
} // namespace gfx

#endif
