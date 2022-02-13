#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "allocator.hpp"
#include <vulkan/vulkan.hpp>

struct GLFWwindow;

namespace gfx {
class Context {
public:
  Context(GLFWwindow *window);
  vk::SurfaceKHR getSurface() const noexcept { return *surface_; }
  vk::PhysicalDevice getPhysicalDevice() const noexcept { return physical_device_; }
  vk::Device getDevice() const noexcept { return *device_; }
  vk::Queue getQueue() const noexcept { return queue_; }
  vk::SwapchainKHR getSwapchain() const noexcept { return *swapchain_; }
  vk::CommandPool getCommandPool() const noexcept { return *command_pool_; }
  vma::Allocator getAllocator() const noexcept { return *allocator_; }

private:
  vk::UniqueInstance instance_;
#ifndef NDEBUG
  vk::UniqueDebugUtilsMessengerEXT messenger_;
#endif
  vk::UniqueSurfaceKHR surface_;
  vk::PhysicalDevice physical_device_;
  uint32_t queue_family_index_ = -1;
  vk::UniqueDevice device_;
  vk::Queue queue_;
  vk::UniqueSwapchainKHR swapchain_;
  vk::UniqueCommandPool command_pool_;
  vma::UniqueAllocator allocator_;
};
} // namespace gfx

#endif