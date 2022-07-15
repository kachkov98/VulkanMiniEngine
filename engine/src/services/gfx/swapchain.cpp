#include "services/gfx/swapchain.hpp"

#include <Tracy.hpp>
#include <spdlog/spdlog.h>

namespace gfx {
void Swapchain::recreate(glm::uvec2 extent, uint32_t num_images, vk::Format format,
                         vk::ColorSpaceKHR color_space, vk::PresentModeKHR present_mode) {
  if (!extent.x || !extent.y)
    return;

  auto surface_capabilities = physical_device_.getSurfaceCapabilitiesKHR(surface_);
  extent_ = vk::Extent2D{std::clamp(extent.x, surface_capabilities.minImageExtent.width,
                                    surface_capabilities.maxImageExtent.width),
                         std::clamp(extent.y, surface_capabilities.minImageExtent.height,
                                    surface_capabilities.maxImageExtent.height)};
  num_images_ = std::max(num_images, surface_capabilities.minImageCount);
  if (surface_capabilities.maxImageCount)
    num_images_ = std::min(num_images_, surface_capabilities.maxImageCount);

  auto surface_formats = physical_device_.getSurfaceFormatsKHR(surface_);
  if (auto it = std::find(surface_formats.begin(), surface_formats.end(),
                          vk::SurfaceFormatKHR{format, color_space});
      it == surface_formats.end())
    throw std::runtime_error("Surface format not supported");
  format_ = format;
  color_space_ = color_space;

  auto present_modes = physical_device_.getSurfacePresentModesKHR(surface_);
  if (auto it = std::find(present_modes.begin(), present_modes.end(), present_mode);
      it == present_modes.end())
    throw std::runtime_error("Present mode not supported");
  present_mode_ = present_mode;

  swapchain_ = device_.createSwapchainKHRUnique({{},
                                                 surface_,
                                                 num_images_,
                                                 format_,
                                                 color_space_,
                                                 extent_,
                                                 1,
                                                 vk::ImageUsageFlagBits::eColorAttachment,
                                                 vk::SharingMode::eExclusive,
                                                 {},
                                                 vk::SurfaceTransformFlagBitsKHR::eIdentity,
                                                 vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                 present_mode_,
                                                 true,
                                                 *swapchain_});

  images_ = device_.getSwapchainImagesKHR(*swapchain_);
  image_views_.clear();
  for (auto image : images_) {
    vk::ImageViewCreateInfo image_view_create_info(
        vk::ImageViewCreateFlags{}, image, vk::ImageViewType::e2D, format_, vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    image_views_.push_back(device_.createImageViewUnique(image_view_create_info));
  }
  spdlog::info(
      "[gfx] [Swapchain] extent: {}x{}, images: {}, format: {}, color space: {}, present mode: {}",
      extent_.width, extent_.height, num_images_, vk::to_string(format_),
      vk::to_string(color_space_), vk::to_string(present_mode_));
}

vk::Result Swapchain::acquireImage(vk::Semaphore image_available) noexcept {
  ZoneScoped;
  return device_.acquireNextImageKHR(*swapchain_, UINT64_MAX, image_available, {}, &current_image_);
}

vk::Result Swapchain::presentImage(vk::Semaphore render_finished) noexcept {
  ZoneScoped;
  vk::PresentInfoKHR present_info{render_finished, *swapchain_, current_image_};
  return present_queue_.presentKHR(&present_info);
}

} // namespace gfx
