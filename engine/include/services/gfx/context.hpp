#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "frame.hpp"
#include "pipelines.hpp"
#include "resources.hpp"
#include "shaders.hpp"
#include "staging_buffer.hpp"
#include "swapchain.hpp"

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

  vk::Device getDevice() const noexcept { return *device_; }

  Swapchain &getSwapchain() noexcept { return swapchain_; }

  DescriptorSetLayoutCache &getDescriptorSetLayoutCache() noexcept {
    return descriptor_set_layout_cache_;
  }
  ShaderModuleCache &getShaderModuleCache() noexcept { return shader_module_cache_; }
  PipelineLayoutCache &getPipelineLayoutCache() noexcept { return pipeline_layout_cache_; }
  PipelineCache &getPipelineCache() noexcept { return pipeline_cache_; }

  BufferDescriptorHeap &getStorageBufferDescriptorHeap() noexcept {
    return storage_buffer_descriptor_heap_;
  }
  ImageDescriptorHeap &getStorageImageDescriptorHeap() noexcept {
    return storage_image_descriptor_heap_;
  }
  ImageDescriptorHeap &getSampledImageDescriptorHeap() noexcept {
    return sampled_image_descriptor_heap_;
  }
  SamplerDescriptorHeap &getSamplerDescriptorHeap() noexcept { return sampler_descriptor_heap_; }

  DescriptorSetAllocator &getDescriptorSetAllocator() noexcept { return descriptor_set_allocator_; }

  vma::Allocator getAllocator() const noexcept { return *allocator_; }

  StagingBuffer &getStagingBuffer() noexcept { return staging_buffer_; }

  Frame &getCurrentFrame() noexcept { return frames_[current_frame_ % frames_in_flight]; }
  void nextFrame() noexcept { allocator_->setCurrentFrameIndex(++current_frame_); }

  void waitIdle() const noexcept { device_->waitIdle(); }

  void flush();

private:
  vk::UniqueInstance instance_ = {};
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_ = {};
#endif
  vk::PhysicalDevice physical_device_ = {};
  std::vector<const char *> enabled_extensions_ = {};

  vk::UniqueSurfaceKHR surface_ = {};

  vk::UniqueDevice device_ = {};
  uint32_t queue_family_index_ = -1u;

  Swapchain swapchain_;

  DescriptorSetLayoutCache descriptor_set_layout_cache_;
  ShaderModuleCache shader_module_cache_;
  PipelineLayoutCache pipeline_layout_cache_;
  PipelineCache pipeline_cache_;

  BufferDescriptorHeap storage_buffer_descriptor_heap_;
  ImageDescriptorHeap storage_image_descriptor_heap_;
  ImageDescriptorHeap sampled_image_descriptor_heap_;
  SamplerDescriptorHeap sampler_descriptor_heap_;

  DescriptorSetAllocator descriptor_set_allocator_;

  vma::UniqueAllocator allocator_ = {};

  StagingBuffer staging_buffer_ = {};

  uint32_t current_frame_ = 0;
  std::array<Frame, frames_in_flight> frames_;
};
} // namespace gfx

#endif
