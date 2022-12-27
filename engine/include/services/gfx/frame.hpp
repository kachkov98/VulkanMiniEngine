#ifndef FRAME_HPP
#define FRAME_HPP

#include "allocator.hpp"
#include "descriptors.hpp"

#include <tracy/TracyVulkan.hpp>

namespace gfx {
class UniqueTracyVkCtx final {
public:
  UniqueTracyVkCtx() = default;
  UniqueTracyVkCtx(vk::PhysicalDevice physical_device, vk::Device device, vk::Queue queue,
                   vk::CommandBuffer command_buffer) {
    tracy_vk_ctx_ = TracyVkContextCalibrated(
        physical_device, device, queue, command_buffer,
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetPhysicalDeviceCalibrateableTimeDomainsEXT,
        VULKAN_HPP_DEFAULT_DISPATCHER.vkGetCalibratedTimestampsEXT);
  }
  UniqueTracyVkCtx(const UniqueTracyVkCtx &) = delete;
  UniqueTracyVkCtx(UniqueTracyVkCtx &&rhs) noexcept : tracy_vk_ctx_(rhs.release()) {}
  ~UniqueTracyVkCtx() {
    if (tracy_vk_ctx_)
      TracyVkDestroy(tracy_vk_ctx_);
  }

  UniqueTracyVkCtx &operator=(const UniqueTracyVkCtx &) = delete;
  UniqueTracyVkCtx &operator=(UniqueTracyVkCtx &&rhs) noexcept {
    reset(rhs.release());
    return *this;
  }

  TracyVkCtx operator*() const noexcept { return tracy_vk_ctx_; }
  TracyVkCtx get() const noexcept { return tracy_vk_ctx_; }

  void reset(const TracyVkCtx tracy_vk_ctx = nullptr) noexcept {
    if (tracy_vk_ctx_ == tracy_vk_ctx)
      return;
    if (tracy_vk_ctx_)
      TracyVkDestroy(tracy_vk_ctx_);
    tracy_vk_ctx_ = tracy_vk_ctx;
  }

  TracyVkCtx release() noexcept {
    TracyVkCtx tracy_vk_ctx = tracy_vk_ctx_;
    tracy_vk_ctx_ = nullptr;
    return tracy_vk_ctx;
  }

private:
  TracyVkCtx tracy_vk_ctx_ = {};
};

class TransientAllocator final {
public:
  TransientAllocator() = default;
  TransientAllocator(vma::Allocator allocator);

  template <typename T>
  std::pair<vk::Buffer, T *> createBuffer(vk::BufferUsageFlags usage, size_t size) {
    auto [buffer, mapped_data] = createBuffer(usage, size * sizeof(T));
    return {buffer, reinterpret_cast<T *>(mapped_data)};
  }
  void reset() { buffers_.clear(); }

private:
  vma::Allocator allocator_ = {};
  vma::UniquePool pool_ = {};
  std::vector<vma::UniqueBuffer> buffers_;

  std::pair<vk::Buffer, void *> createBuffer(vk::BufferUsageFlags usage, size_t size);
};

class Frame final {
public:
  Frame() = default;
  Frame(vk::PhysicalDevice physical_device, vk::Device device, uint32_t queue_family_index,
        uint32_t queue_index, vma::Allocator allocator);

  vk::Semaphore getImageAvailableSemaphore() const noexcept { return *image_available_; }
  vk::Semaphore getRenderFinishedSemaphore() const noexcept { return *render_finished_; }
  vk::CommandPool getCommandPool() const noexcept { return *command_pool_; }
  vk::CommandBuffer getCommandBuffer() const noexcept { return *command_buffer_; }
  TracyVkCtx getTracyVkCtx() const noexcept { return *tracy_vk_ctx_; }
  TransientAllocator &getAllocator() noexcept { return transient_allocator_; }

  void submit() const;
  void reset();

private:
  vk::Device device_ = {};
  vk::Queue queue_ = {};
  vk::UniqueSemaphore image_available_ = {}, render_finished_ = {};
  vk::UniqueFence render_fence_ = {};
  vk::UniqueCommandPool command_pool_ = {};
  vk::UniqueCommandBuffer command_buffer_ = {};
  UniqueTracyVkCtx tracy_vk_ctx_ = {};
  TransientAllocator transient_allocator_;
};
} // namespace gfx

#endif
