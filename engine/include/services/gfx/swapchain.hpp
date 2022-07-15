#ifndef SWAPCHAIN_HPP
#define SWAPCHAIN_HPP

#include <glm/vec2.hpp>
#include <vulkan/vulkan.hpp>

namespace gfx {
class Swapchain {
public:
  Swapchain() = default;
  Swapchain(vk::PhysicalDevice physical_device, vk::SurfaceKHR surface, vk::Device device,
            uint32_t queue_family_index, uint32_t queue_index)
      : physical_device_(physical_device), surface_(surface), device_(device),
        present_queue_(device.getQueue(queue_family_index, queue_index)) {}

  void recreate(glm::uvec2 extent, uint32_t num_images = 3,
                vk::Format format = vk::Format::eB8G8R8A8Unorm,
                vk::ColorSpaceKHR color_space = vk::ColorSpaceKHR::eSrgbNonlinear,
                vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo);

  vk::Extent2D getExtent() const noexcept { return extent_; };
  uint32_t getNumImages() const noexcept { return num_images_; }
  vk::Format getFormat() const noexcept { return format_; }
  vk::ColorSpaceKHR getColorSpace() const noexcept { return color_space_; }
  vk::PresentModeKHR getPresentMode() const noexcept { return present_mode_; }

  vk::Image getCurrentImage() const noexcept { return images_[current_image_]; }
  vk::ImageView getCurrentImageView() const noexcept { return *image_views_[current_image_]; }

  vk::Result acquireImage(vk::Semaphore image_available) noexcept;
  vk::Result presentImage(vk::Semaphore render_finished) noexcept;

private:
  vk::PhysicalDevice physical_device_ = {};
  vk::SurfaceKHR surface_ = {};
  vk::Device device_ = {};
  vk::Queue present_queue_ = {};

  vk::Extent2D extent_ = {};
  uint32_t num_images_{};
  vk::Format format_ = {};
  vk::ColorSpaceKHR color_space_ = {};
  vk::PresentModeKHR present_mode_ = {};

  vk::UniqueSwapchainKHR swapchain_ = {};

  uint32_t current_image_{};
  std::vector<vk::Image> images_{};
  std::vector<vk::UniqueImageView> image_views_{};
};
} // namespace gfx

#endif
