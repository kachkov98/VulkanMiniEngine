#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "allocator.hpp"
#include "descriptors.hpp"
#include "pipelines.hpp"
#include "shaders.hpp"

#include <vulkan/vulkan.hpp>

#include <TracyVulkan.hpp>

#include <string_view>

namespace wsi {
class Window;
}

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

class Frame final {
public:
  Frame() = default;
  Frame(vk::PhysicalDevice physical_device, vk::Device device, vk::Queue queue,
        uint32_t queue_family_index, vma::Allocator allocator);

  void submit() const;
  void reset() const;

  vk::Semaphore getImageAvailableSemaphore() const noexcept { return *image_available_; }
  vk::Semaphore getRenderFinishedSemaphore() const noexcept { return *render_finished_; }
  vk::CommandPool getCommandPool() const noexcept { return *command_pool_; }
  vk::CommandBuffer getCommandBuffer() const noexcept { return *command_buffer_; }
  vma::Pool getMemoryPool() const noexcept { return *memory_pool_; }
  TracyVkCtx getTracyVkCtx() const noexcept { return *tracy_vk_ctx_; }

private:
  vk::Device device_ = {};
  vk::Queue queue_ = {};
  vma::Allocator allocator_ = {};
  vk::UniqueSemaphore image_available_ = {}, render_finished_ = {};
  vk::UniqueFence render_fence_ = {};
  vk::UniqueCommandPool command_pool_ = {};
  vk::UniqueCommandBuffer command_buffer_ = {};
  vma::UniquePool memory_pool_ = {};
  UniqueTracyVkCtx tracy_vk_ctx_ = {};
};

class Context final {
public:
  static constexpr unsigned frames_in_flight = 3;

  Context(const wsi::Window &window);

  vk::PhysicalDevice getPhysicalDevice() const noexcept { return physical_device_; }
  bool isExtensionEnabled(std::string_view name) const noexcept;

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
  bool acquireNextImage(vk::Semaphore image_available);
  bool presentImage(vk::Semaphore render_finished);

  ShaderModuleCache &getShaderModuleCache() noexcept { return shader_module_cache_; }
  DescriptorSetLayoutCache &getDescriptorSetLayoutCache() noexcept {
    return descriptor_set_layout_cache_;
  }
  PipelineLayoutCache &getPipelineLayoutCache() noexcept { return pipeline_layout_cache_; }
  PipelineCache &getPipelineCache() noexcept { return pipeline_cache_; }

  vma::Allocator getAllocator() const noexcept { return *allocator_; }

  Frame &getCurrentFrame() noexcept { return frames_[current_frame_ % frames_in_flight]; }
  void nextFrame() noexcept { allocator_->setCurrentFrameIndex(++current_frame_); }

  void waitIdle() const noexcept { device_->waitIdle(); }

private:
  const wsi::Window *window_ = nullptr;
  vk::UniqueInstance instance_ = {};
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_ = {};
#endif
  vk::PhysicalDevice physical_device_ = {};
  std::vector<const char *> enabled_extensions_ = {};

  vk::UniqueSurfaceKHR surface_ = {};
  vk::SurfaceFormatKHR surface_format_ = {};

  vk::UniqueDevice device_ = {};
  uint32_t main_queue_family_index_ = -1u;

  uint32_t current_swapchain_image_{0}, num_swapchain_images_{3};
  vk::Extent2D swapchain_extent_ = {};
  vk::UniqueSwapchainKHR swapchain_ = {};
  std::vector<vk::Image> swapchain_images_{};
  std::vector<vk::UniqueImageView> swapchain_image_views_{};

  DescriptorSetLayoutCache descriptor_set_layout_cache_;
  ShaderModuleCache shader_module_cache_;
  PipelineLayoutCache pipeline_layout_cache_;
  PipelineCache pipeline_cache_;

  vma::UniqueAllocator allocator_ = {};

  uint32_t current_frame_ = 0;
  std::array<Frame, frames_in_flight> frames_;

  void recreateSwapchain();
  bool checkSwapchainResult(vk::Result result);
};
} // namespace gfx

#endif
