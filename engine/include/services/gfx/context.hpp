#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "allocator.hpp"
#include "descriptors.hpp"
#include <vulkan/vulkan.hpp>

namespace wsi {
class Window;
}

namespace gfx {

class Frame {
public:
  Frame() = default;
  Frame(vk::Device device, uint32_t queue_family_index);
  vk::CommandPool getCommandPool() const noexcept { return *command_pool_; }
  vk::CommandBuffer getCommandBuffer() const noexcept { return *command_buffer_; }
  vma::Pool getMemoryPool() const noexcept { return *memory_pool_; }

private:
  vk::Device device_;
  vk::UniqueSemaphore image_available_, render_finished_;
  vk::UniqueFence render_fence_;
  vk::UniqueCommandPool command_pool_;
  vk::UniqueCommandBuffer command_buffer_;
  vma::UniquePool memory_pool_;
};

class Context final {
public:
  static constexpr unsigned frames_in_flight = 2;

  Context(const wsi::Window &window);
  vk::SurfaceKHR getSurface() const noexcept { return *surface_; }
  vk::PhysicalDevice getPhysicalDevice() const noexcept { return physical_device_; }
  vk::Device getDevice() const noexcept { return *device_; }
  vk::Queue getMainQueue() const noexcept { return device_->getQueue(main_queue_family_index_, 0); }
  uint32_t getMainQueueFamilyIndex() const noexcept { return main_queue_family_index_; }
  vk::SwapchainKHR getSwapchain() const noexcept { return *swapchain_; }
  vma::Allocator getAllocator() const noexcept { return *allocator_; }

  Frame &getCurrentFrame() noexcept { return frames_[current_frame_]; }

private:
  const wsi::Window *window_ = nullptr;
  vk::UniqueInstance instance_ = {};
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_ = {};
#endif
  vk::UniqueSurfaceKHR surface_ = {};
  vk::PhysicalDevice physical_device_ = {};
  vk::UniqueDevice device_ = {};
  uint32_t main_queue_family_index_ = -1u;
  vk::UniqueSwapchainKHR swapchain_ = {};
  vma::UniqueAllocator allocator_ = {};

  unsigned current_frame_ = 0;
  std::array<Frame, frames_in_flight> frames_;
};
} // namespace gfx

#endif
