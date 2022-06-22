#ifndef STAGING_BUFFER_HPP
#define STAGING_BUFFER_HPP

#include "allocator.hpp"

#include <variant>
#include <vector>

namespace gfx {

class StagingBuffer {
public:
  static constexpr size_t max_size = 128 * 1024 * 1024;

  template <typename T> using Data = vk::ArrayProxy<T>;

  StagingBuffer() = default;
  StagingBuffer(vk::Device device, vk::Queue queue, uint32_t queue_family_index,
                vma::Allocator allocator);
  StagingBuffer(const StagingBuffer &) = delete;
  StagingBuffer(StagingBuffer &&) = delete;
  StagingBuffer &operator=(const StagingBuffer &) = delete;
  StagingBuffer &operator=(StagingBuffer &&) = default;

  template <typename T>
  void uploadBuffer(vk::Buffer buffer, const Data<T> &data,
                    const vk::ArrayProxy<const vk::BufferCopy2> &regions) {
    auto offset = copyData(reinterpret_cast<const void *>(data.data()), data.size() * sizeof(T));
    std::vector<vk::BufferCopy2> copies(regions.begin(), regions.end());
    for (auto &copy : copies)
      copy.srcOffset += offset;
    copies_.push_back(BufferCopy{buffer, std::move(copies)});
  }

  template <typename T>
  void uploadImage(vk::Image image, vk::ImageLayout old_layout, vk::ImageLayout new_layout,
                   vk::ImageSubresourceRange subresource, const Data<T> &data,
                   const vk::ArrayProxy<const vk::BufferImageCopy2> &regions) {
    auto offset = copyData(reinterpret_cast<const void *>(data.data()), data.size() * sizeof(T));
    std::vector<vk::BufferImageCopy2> copies(regions.begin(), regions.end());
    for (auto &copy : copies)
      copy.bufferOffset += offset;
    copies_.push_back(ImageCopy{image, old_layout, new_layout, subresource, std::move(copies)});
  }

  void flush();

private:
  vk::Device device_ = {};
  vk::Queue queue_ = {};
  vk::UniqueFence upload_fence_ = {};
  vk::UniqueCommandPool command_pool_ = {};
  vk::UniqueCommandBuffer command_buffer_ = {};
  vma::UniqueBuffer staging_buffer_ = {};

  void *mapped_data_ = nullptr;
  size_t offset_ = 0;

  struct BufferCopy {
    vk::Buffer buffer;
    std::vector<vk::BufferCopy2> regions;
  };

  struct ImageCopy {
    vk::Image image;
    vk::ImageLayout old_layout, new_layout;
    vk::ImageSubresourceRange subresource;
    std::vector<vk::BufferImageCopy2> regions;
  };

  using Copy = std::variant<BufferCopy, ImageCopy>;
  std::vector<Copy> copies_;

  friend class CmdBufGenerator;

  size_t copyData(const void *data, size_t size);
};
} // namespace gfx

#endif
