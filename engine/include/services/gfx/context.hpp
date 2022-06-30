#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "frame.hpp"
#include "pipelines.hpp"
#include "shaders.hpp"
#include "staging_buffer.hpp"

#include <glm/vec2.hpp>
#include <vulkan/vulkan.hpp>

#include <string_view>

namespace wsi {
class Window;
}

namespace gfx {
class Context final {
public:
  static constexpr unsigned frames_in_flight = 3;

  Context(const wsi::Window &window);

  vk::PhysicalDevice getPhysicalDevice() const noexcept { return physical_device_; }
  bool isExtensionEnabled(std::string_view name) const noexcept;

  vk::SurfaceKHR getSurface() const noexcept { return *surface_; }
  vk::SurfaceFormatKHR getSurfaceFormat() const noexcept { return surface_format_; }

  vk::Device getDevice() const noexcept { return *device_; }

  void recreateSwapchain(glm::uvec2 new_extent);
  vk::Image getCurrentImage() const noexcept { return swapchain_images_[current_swapchain_image_]; }
  vk::ImageView getCurrentImageView() const noexcept {
    return *swapchain_image_views_[current_swapchain_image_];
  }
  vk::Extent2D getSwapchainExtent() const noexcept { return swapchain_extent_; };
  vk::Result acquireNextImage(vk::Semaphore image_available) noexcept;
  vk::Result presentImage(vk::Semaphore render_finished) const noexcept;

  DescriptorSetLayoutCache &getDescriptorSetLayoutCache() noexcept {
    return descriptor_set_layout_cache_;
  }
  ShaderModuleCache &getShaderModuleCache() noexcept { return shader_module_cache_; }
  PipelineLayoutCache &getPipelineLayoutCache() noexcept { return pipeline_layout_cache_; }
  PipelineCache &getPipelineCache() noexcept { return pipeline_cache_; }

  ResourceDescriptorHeap &getBufferDescriptorHeap() noexcept { return buffer_descriptor_heap_; }
  ResourceDescriptorHeap &getImageDescriptorHeap() noexcept { return image_descriptor_heap_; }
  ResourceDescriptorHeap &getTextureDescriptorHeap() noexcept { return texture_descriptor_heap_; }
  ResourceDescriptorHeap &getSamplerDescriptorHeap() noexcept { return sampler_descriptor_heap_; }

  vma::Allocator getAllocator() const noexcept { return *allocator_; }

  StagingBuffer &getStagingBuffer() noexcept { return staging_buffer_; }

  Frame &getCurrentFrame() noexcept { return frames_[current_frame_ % frames_in_flight]; }
  void nextFrame() noexcept { allocator_->setCurrentFrameIndex(++current_frame_); }

  void waitIdle() const noexcept { device_->waitIdle(); }

private:
  vk::UniqueInstance instance_ = {};
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_ = {};
#endif
  vk::PhysicalDevice physical_device_ = {};
  std::vector<const char *> enabled_extensions_ = {};

  vk::UniqueSurfaceKHR surface_ = {};
  vk::SurfaceFormatKHR surface_format_ = {};

  vk::UniqueDevice device_ = {};
  uint32_t queue_family_index_ = -1u;

  uint32_t current_swapchain_image_{0}, num_swapchain_images_{3};
  vk::Extent2D swapchain_extent_ = {};
  vk::UniqueSwapchainKHR swapchain_ = {};
  std::vector<vk::Image> swapchain_images_{};
  std::vector<vk::UniqueImageView> swapchain_image_views_{};

  DescriptorSetLayoutCache descriptor_set_layout_cache_;
  ShaderModuleCache shader_module_cache_;
  PipelineLayoutCache pipeline_layout_cache_;
  PipelineCache pipeline_cache_;

  ResourceDescriptorHeap buffer_descriptor_heap_;
  ResourceDescriptorHeap image_descriptor_heap_;
  ResourceDescriptorHeap texture_descriptor_heap_;
  ResourceDescriptorHeap sampler_descriptor_heap_;

  vma::UniqueAllocator allocator_ = {};

  StagingBuffer staging_buffer_ = {};

  uint32_t current_frame_ = 0;
  std::array<Frame, frames_in_flight> frames_;
};
} // namespace gfx

#endif
