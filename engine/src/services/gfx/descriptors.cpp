#include "services/gfx/descriptors.hpp"

namespace gfx {
static constexpr std::pair<vk::DescriptorType, float> descriptor_sizes[] = {
    {vk::DescriptorType::eSampler, 0.5f},
    {vk::DescriptorType::eCombinedImageSampler, 4.f},
    {vk::DescriptorType::eSampledImage, 4.f},
    {vk::DescriptorType::eStorageImage, 1.f},
    {vk::DescriptorType::eUniformTexelBuffer, 1.f},
    {vk::DescriptorType::eStorageTexelBuffer, 1.f},
    {vk::DescriptorType::eUniformBuffer, 2.f},
    {vk::DescriptorType::eStorageBuffer, 2.f},
    {vk::DescriptorType::eUniformBufferDynamic, 1.f},
    {vk::DescriptorType::eStorageBufferDynamic, 1.f},
    {vk::DescriptorType::eInputAttachment, 0.5f}};

vk::DescriptorPool DescriptorSetAllocator::getPool(unsigned size) {
  if (free_pools_.empty()) {
    std::vector<vk::DescriptorPoolSize> pool_sizes;
    pool_sizes.reserve(std::size(descriptor_sizes));
    for (const auto &desc : descriptor_sizes)
      pool_sizes.emplace_back(desc.first, static_cast<uint32_t>(size * desc.second));
    used_pools_.push_back(device_.createDescriptorPoolUnique({{}, size, pool_sizes}));
  } else {
    auto pool = std::move(free_pools_.back());
    free_pools_.pop_back();
    used_pools_.push_back(std::move(pool));
  }
  return *used_pools_.back();
}

vk::UniqueDescriptorSet
DescriptorSetAllocator::allocate(const vk::DescriptorSetLayout &descriptor_layout,
                                 const vk::ArrayProxy<const vk::WriteDescriptorSet> &bindings) {
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
  std::vector<vk::WriteDescriptorSet> writes(bindings.begin(), bindings.end());
  for (auto &write : writes) {
    assert(!write.dstSet);
    write.dstSet = descriptor_set;
  }
  device_.updateDescriptorSets(writes, {});
  return vk::UniqueDescriptorSet(descriptor_set, deleter);
}

void DescriptorSetAllocator::reset() {
  for (const auto &pool : used_pools_)
    device_.resetDescriptorPool(*pool);
  free_pools_.insert(free_pools_.end(), std::make_move_iterator(used_pools_.begin()),
                     std::make_move_iterator(used_pools_.end()));
  used_pools_.clear();
  current_pool_ = nullptr;
}

vk::UniqueDescriptorSet DescriptorSetBuilder::build() {
  return descriptor_allocator_->allocate(DescriptorSetLayoutBuilder::build(), bindings_);
}

ResourceDescriptorHeap::ResourceDescriptorHeap(vk::Device device, vk::DescriptorType type,
                                               uint32_t size, uint32_t binding)
    : device_(device), type_(type), size_(size), binding_(binding) {
  vk::DescriptorPoolSize pool_size{type, size};
  pool_ = device_.createDescriptorPoolUnique(
      {vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind, 1, pool_size});
  vk::DescriptorSetLayoutBinding layout_binding{binding_, type_, size_,
                                                vk::ShaderStageFlagBits::eAll};
  vk::DescriptorBindingFlags binding_flags =
      vk::DescriptorBindingFlagBits::eUpdateAfterBind |
      vk::DescriptorBindingFlagBits::eUpdateUnusedWhilePending |
      vk::DescriptorBindingFlagBits::ePartiallyBound;
  descriptor_set_layout_ = device_.createDescriptorSetLayoutUnique(vk::StructureChain{
      vk::DescriptorSetLayoutCreateInfo{vk::DescriptorSetLayoutCreateFlagBits::eUpdateAfterBindPool,
                                        layout_binding},
      vk::DescriptorSetLayoutBindingFlagsCreateInfo{binding_flags}}.get());
  descriptor_set_ = device_.allocateDescriptorSets({*pool_, *descriptor_set_layout_}).front();
  reset();
}

void ResourceDescriptorHeap::flush() {
  std::vector<vk::WriteDescriptorSet> writes;
  writes.reserve(descriptors_.size());
  for (const auto &[id, descriptor_info] : descriptors_) {
    writes.emplace_back(descriptor_set_, binding_, id, 1, type_,
                        std::get_if<vk::DescriptorImageInfo>(&descriptor_info),
                        std::get_if<vk::DescriptorBufferInfo>(&descriptor_info));
  }
  device_.updateDescriptorSets(writes, {});
  descriptors_.clear();
};
void ResourceDescriptorHeap::reset() {
  descriptors_.clear();
  free_list_.clear();
  free_list_.reserve(size_);
  for (uint32_t i = 0; i < size_; ++i)
    free_list_.push_back(size_ - i - 1);
}

uint32_t ResourceDescriptorHeap::allocate() {
  if (free_list_.empty())
    throw std::runtime_error("Resource heap exceeded");
  auto id = free_list_.back();
  free_list_.pop_back();
  return id;
}

} // namespace gfx
