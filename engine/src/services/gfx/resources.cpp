#include "services/gfx/resources.hpp"

namespace gfx {

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

uint32_t Buffer::allocate(BufferDescriptorHeap &heap, const BufferView &view) {
  handles_.push_back(heap.allocateUnique(get(), view.offset, view.range));
  return handles_.back().get();
}

uint32_t Image::allocate(ImageDescriptorHeap &heap, const ImageView &view, vk::ImageLayout layout) {
  auto image_view = vk::Device{image_.getOwner().getInfo().device}.createImageViewUnique(
      {{}, get(), view.view_type, view.format, view.component_mapping, view.subresource_range});
  auto handle = heap.allocateUnique(*image_view, layout);
  handles_.emplace_back(std::move(image_view), std::move(handle));
  return handles_.back().second.get();
}

uint32_t Sampler::allocate(SamplerDescriptorHeap &heap) {
  handles_.push_back(heap.allocateUnique(get()));
  return handles_.back().get();
}

} // namespace gfx
