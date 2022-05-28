#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "allocator.hpp"
#include "descriptors.hpp"

#include <vulkan/vulkan.hpp>

namespace wsi {
class Window;
}

namespace gfx {

class Frame final {
public:
  Frame() = default;
  Frame(vk::Device device, vma::Allocator allocator, uint32_t queue_family_index);

  void submit(vk::Queue queue) const;
  void reset() const;

  vk::Semaphore getImageAvailableSemaphore() const noexcept { return *image_available_; }
  vk::Semaphore getRenderFinishedSemaphore() const noexcept { return *render_finished_; }
  vk::CommandPool getCommandPool() const noexcept { return *command_pool_; }
  vk::CommandBuffer getCommandBuffer() const noexcept { return *command_buffer_; }
  vma::Pool getMemoryPool() const noexcept { return *memory_pool_; }

private:
  vk::Device device_;
  vma::Allocator allocator_;
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

  vk::PhysicalDevice getPhysicalDevice() const noexcept { return physical_device_; }

  vk::SurfaceKHR getSurface() const noexcept { return *surface_; }
  vk::SurfaceFormatKHR getSurfaceFormat() const noexcept { return surface_format_; }

  vk::Device getDevice() const noexcept { return *device_; }
  vk::Queue getMainQueue() const noexcept { return device_->getQueue(main_queue_family_index_, 0); }
  uint32_t getMainQueueFamilyIndex() const noexcept { return main_queue_family_index_; }

  vk::Image getCurrentImage() const noexcept { return swapchain_images_[current_swapchain_image_]; }
  vk::ImageView getCurrentImageView() const noexcept {
    return *swapchain_image_views_[current_swapchain_image_];
  }
  vk::Extent2D getSwapchainExtent() const noexcept { return swapchain_extent_; };
  void acquireNextImage(vk::Semaphore image_available);
  void presentImage(vk::Semaphore render_finished);
  void recreateSwapchain();

  vk::PipelineCache getPipelineCache() const noexcept { return *pipeline_cache_; }
  void savePipelineCache() const;

  vma::Allocator getAllocator() const noexcept { return *allocator_; }

  Frame &getCurrentFrame() noexcept { return frames_[current_frame_]; }
  void nextFrame() noexcept { current_frame_ = (current_frame_ + 1) % frames_in_flight; }

  void waitIdle() const noexcept { device_->waitIdle(); }

private:
  const wsi::Window *window_ = nullptr;
  vk::UniqueInstance instance_ = {};
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_ = {};
#endif
  vk::PhysicalDevice physical_device_ = {};

  vk::UniqueSurfaceKHR surface_ = {};
  vk::SurfaceFormatKHR surface_format_ = {};
  vk::SurfaceCapabilitiesKHR surface_capabilities_ = {};

  vk::UniqueDevice device_ = {};
  uint32_t main_queue_family_index_ = -1u;

  uint32_t current_swapchain_image_{0}, num_swapchain_images_{3};
  vk::Extent2D swapchain_extent_ = {};
  vk::UniqueSwapchainKHR swapchain_ = {};
  std::vector<vk::Image> swapchain_images_{};
  std::vector<vk::UniqueImageView> swapchain_image_views_{};

  vk::UniquePipelineCache pipeline_cache_ = {};

  vma::UniqueAllocator allocator_ = {};

  unsigned current_frame_ = 0;
  std::array<Frame, frames_in_flight> frames_;
};
} // namespace gfx

#endif
