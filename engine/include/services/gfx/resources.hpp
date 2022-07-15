#ifndef RESOURCES_HPP
#define RESOURCES_HPP

#include "allocator.hpp"
#include "staging_buffer.hpp"

#include <variant>
#include <vector>

namespace gfx {
class ResourceDescriptorHeap {
public:
  class UniqueHandle {
  public:
    UniqueHandle() = default;
    UniqueHandle(ResourceDescriptorHeap &owner, uint32_t index) : owner_(&owner), index_(index) {}
    UniqueHandle(const UniqueHandle &rhs) = delete;
    UniqueHandle(UniqueHandle &&rhs) noexcept : owner_(rhs.owner_), index_(rhs.release()) {}
    UniqueHandle &operator=(const UniqueHandle &rhs) = delete;
    UniqueHandle &operator=(UniqueHandle &&rhs) noexcept {
      owner_ = rhs.owner_;
      reset(rhs.release());
      return *this;
    }
    ~UniqueHandle() {
      if (index_ != UINT32_MAX)
        owner_->free(index_);
    }

    uint32_t get() const noexcept { return index_; }

    void reset(uint32_t index = UINT32_MAX) noexcept {
      if (index_ == index)
        return;
      if (index_ != UINT32_MAX)
        owner_->free(index_);
      index_ = index;
    }
    uint32_t release() noexcept {
      auto index = index_;
      index_ = UINT32_MAX;
      return index;
    }

  private:
    ResourceDescriptorHeap *owner_ = nullptr;
    uint32_t index_ = UINT32_MAX;
  };

  ResourceDescriptorHeap() = default;
  ResourceDescriptorHeap(vk::Device device, vk::DescriptorType type, uint32_t size,
                         uint32_t binding = 0);

  vk::DescriptorSet get() const noexcept { return descriptor_set_; }
  vk::DescriptorSetLayout getLayout() const noexcept { return *descriptor_set_layout_; }

  void free(uint32_t id) noexcept { free_list_.push_back(id); }

  void flush();
  void reset();

protected:
  using DescriptorInfo = std::variant<vk::DescriptorBufferInfo, vk::DescriptorImageInfo>;
  std::vector<std::pair<uint32_t, DescriptorInfo>> descriptors_;

  uint32_t allocate();

private:
  vk::Device device_;
  vk::DescriptorType type_;
  uint32_t size_;
  uint32_t binding_;

  vk::UniqueDescriptorPool pool_;
  vk::UniqueDescriptorSetLayout descriptor_set_layout_;
  vk::DescriptorSet descriptor_set_;

  std::vector<uint32_t> free_list_;
};

class BufferDescriptorHeap final : public ResourceDescriptorHeap {
public:
  BufferDescriptorHeap() = default;
  BufferDescriptorHeap(vk::Device device, vk::DescriptorType type, uint32_t size,
                       uint32_t binding = 0)
      : ResourceDescriptorHeap(device, type, size, binding){};

  uint32_t allocate(vk::Buffer buffer, vk::DeviceSize offset = 0,
                    vk::DeviceSize range = VK_WHOLE_SIZE) {
    auto id = ResourceDescriptorHeap::allocate();
    descriptors_.emplace_back(id, vk::DescriptorBufferInfo{buffer, offset, range});
    return id;
  }
  UniqueHandle allocateUnique(vk::Buffer buffer, vk::DeviceSize offset = 0,
                              vk::DeviceSize range = VK_WHOLE_SIZE) {
    return UniqueHandle(*this, allocate(buffer, offset, range));
  }
};

class ImageDescriptorHeap final : public ResourceDescriptorHeap {
public:
  ImageDescriptorHeap() = default;
  ImageDescriptorHeap(vk::Device device, vk::DescriptorType type, uint32_t size,
                      uint32_t binding = 0)
      : ResourceDescriptorHeap(device, type, size, binding){};

  uint32_t allocate(vk::ImageView image_view, vk::ImageLayout image_layout) {
    auto id = ResourceDescriptorHeap::allocate();
    descriptors_.emplace_back(id, vk::DescriptorImageInfo{{}, image_view, image_layout});
    return id;
  }
  UniqueHandle allocateUnique(vk::ImageView image_view, vk::ImageLayout image_layout) {
    return UniqueHandle(*this, allocate(image_view, image_layout));
  }
};

class SamplerDescriptorHeap final : public ResourceDescriptorHeap {
public:
  SamplerDescriptorHeap() = default;
  SamplerDescriptorHeap(vk::Device device, vk::DescriptorType type, uint32_t size,
                        uint32_t binding = 0)
      : ResourceDescriptorHeap(device, type, size, binding){};

  uint32_t allocate(vk::Sampler sampler) {
    auto id = ResourceDescriptorHeap::allocate();
    descriptors_.emplace_back(id, vk::DescriptorImageInfo{sampler});
    return id;
  }
  UniqueHandle allocateUnique(vk::Sampler sampler) {
    return UniqueHandle(*this, allocate(sampler));
  }
};

struct BufferView {
  vk::DeviceSize offset{0};
  vk::DeviceSize range{VK_WHOLE_SIZE};
};

class Buffer final {
public:
  Buffer() = default;
  Buffer(vma::Allocator allocator, const vk::BufferCreateInfo &buffer_info)
      : buffer_(allocator.createBufferUnique(buffer_info, {{}, VMA_MEMORY_USAGE_AUTO})) {}

  vk::Buffer get() { return buffer_->getBuffer(); }

  uint32_t allocate(BufferDescriptorHeap &heap, const BufferView &view);

  template <typename T>
  void upload(StagingBuffer &staging_buffer, const StagingBuffer::Data<T> &data,
              const vk::ArrayProxy<const vk::BufferCopy2> &regions) {
    staging_buffer.uploadBuffer(get(), data, regions);
  }

private:
  vma::UniqueBuffer buffer_;
  // TODO: SmallVector (usually number of allocated descriptors is 1)
  std::vector<ResourceDescriptorHeap::UniqueHandle> handles_;
};

struct ImageView {
  vk::ImageViewType view_type;
  vk::Format format;
  vk::ComponentMapping component_mapping;
  vk::ImageSubresourceRange subresource_range;
};

class Image final {
public:
  Image() = default;
  Image(vma::Allocator allocator, const vk::ImageCreateInfo &image_info)
      : image_(allocator.createImageUnique(image_info, {{}, VMA_MEMORY_USAGE_AUTO})) {}

  vk::Image get() { return image_->getImage(); }

  uint32_t allocate(ImageDescriptorHeap &heap, const ImageView &view, vk::ImageLayout layout);

  template <typename T>
  void upload(StagingBuffer &staging_buffer, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
              vk::ImageSubresourceRange subresource, const StagingBuffer::Data<T> &data,
              const vk::ArrayProxy<const vk::BufferImageCopy2> &regions) {
    staging_buffer.uploadImage(get(), old_layout, new_layout, subresource, data, regions);
  }

private:
  vma::UniqueImage image_;
  // TODO: SmallVector (usually number of allocated descriptors is 1)
  std::vector<std::pair<vk::UniqueImageView, ResourceDescriptorHeap::UniqueHandle>> handles_;
};

class Sampler final {
public:
  Sampler() = default;
  Sampler(vk::Device device, const vk::SamplerCreateInfo &sampler_info)
      : sampler_(device.createSamplerUnique(sampler_info)) {}

  vk::Sampler get() { return *sampler_; }

  uint32_t allocate(SamplerDescriptorHeap &heap);

private:
  vk::UniqueSampler sampler_;
  // TODO: SmallVector (usually number of allocated descriptors is 1)
  std::vector<ResourceDescriptorHeap::UniqueHandle> handles_;
};
} // namespace gfx

#endif
