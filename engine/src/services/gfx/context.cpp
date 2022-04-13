#include "services/gfx/context.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include <fstream>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace gfx {
static std::vector<const char *> getInstanceExtensions() {
  uint32_t count;
  const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char *> extensions(glfw_extensions, glfw_extensions + count);
#ifndef NDEBUG
  extensions.emplace_back("VK_EXT_debug_utils");
#endif
  return extensions;
}

static std::vector<const char *> getDeviceExtensions() { return {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; }

static std::vector<const char *> getValidationLayers() {
#ifdef NDEBUG
  return {};
#else
  return {"VK_LAYER_KHRONOS_validation"};
#endif
}

#ifndef NDEBUG
static spdlog::level::level_enum getLogLevel(VkDebugUtilsMessageSeverityFlagBitsEXT level) {
  switch (level) {
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
    return spdlog::level::err;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
    return spdlog::level::warn;
  case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
    return spdlog::level::info;
  default:
    return spdlog::level::off;
  }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {
  spdlog::log(getLogLevel(messageSeverity), "[vulkan] {}", pCallbackData->pMessage);
  return VK_FALSE;
}
#endif

const std::filesystem::path Context::cache_path =
    std::filesystem::current_path() / "shader_cache.bin";

Context::Context(const wsi::Window &window) : window_(&window) {
  // Create instance
  {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(glfwGetInstanceProcAddress);
    vk::ApplicationInfo app_info{"VulkanMiniEngine", VK_MAKE_VERSION(0, 0, 1), "VulkanMiniEngine",
                                 VK_MAKE_VERSION(0, 0, 1), VK_API_VERSION_1_3};
    auto layers = getValidationLayers();
    auto extensions = getInstanceExtensions();
    instance_ = vk::createInstanceUnique({{}, &app_info, layers, extensions});
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance_);
  }
  // Create debug messenger
#ifndef NDEBUG
  messenger_ = instance_->createDebugUtilsMessengerEXTUnique(
      {{},
       vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
           vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
           vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
       debugCallback});
#endif
  // Create physical device
  {
    auto physical_devices = instance_->enumeratePhysicalDevices();
    if (physical_devices.empty())
      throw std::runtime_error("No supported vulkan physical devices");
    spdlog::info("[gfx] Supported devices:");
    for (const auto &physical_device : physical_devices)
      spdlog::info("[gfx]     {}", physical_device.getProperties().deviceName);
    physical_device_ = physical_devices[0];
  }
  // Create surface
  {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*instance_, window_->getHandle(), nullptr, &surface) != VK_SUCCESS)
      throw std::runtime_error("glfwCreateWindowSurface failed");
    surface_ = vk::UniqueSurfaceKHR(surface, *instance_);
    surface_format_ =
        vk::SurfaceFormatKHR{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    std::vector<vk::SurfaceFormatKHR> formats = physical_device_.getSurfaceFormatsKHR(*surface_);
    if (std::none_of(formats.begin(), formats.end(),
                     [&](const auto &surface_format) { return surface_format == surface_format_; }))
      throw std::runtime_error("Default format (B8G8R8A8_SRGB) not supported by surface");
    surface_capabilities_ = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
  }
  // Create device and queue
  {
    auto queue_family_properties = physical_device_.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queue_family_properties.size(); ++i)
      if ((queue_family_properties[i].queueFlags &
           (vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eGraphics)) &&
          physical_device_.getSurfaceSupportKHR(i, *surface_)) {
        main_queue_family_index_ = i;
        break;
      }
    if (main_queue_family_index_ == -1)
      throw std::runtime_error("No device queue with compute, graphics and present support");
    std::array<float, 1> queue_priorities{1.0f};
    vk::DeviceQueueCreateInfo queue_create_info{{}, main_queue_family_index_, queue_priorities};
    auto extensions = getDeviceExtensions();
    device_ = physical_device_.createDeviceUnique({{}, queue_create_info, {}, extensions});
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device_);
  }
  // Create swapchain
  {
    num_swapchain_images_ = std::clamp(num_swapchain_images_, surface_capabilities_.minImageCount,
                                       surface_capabilities_.maxImageCount);
    recreateSwapchain();
    swapchain_images_ = device_->getSwapchainImagesKHR(*swapchain_);
  }
  // Load pipeline cache
  {
    spdlog::info("[gfx] Loading shader cache from disk");
    std::ifstream f(cache_path, std::ios::in | std::ios::binary);
    std::vector<uint8_t> cache_data{std::istreambuf_iterator<char>(f),
                                    std::istreambuf_iterator<char>()};
    pipeline_cache_ =
        device_->createPipelineCacheUnique({{}, cache_data.size(), cache_data.data()});
  }
  // Create allocator
  {
    VmaVulkanFunctions vma_vk_funcs{};
    vma_vk_funcs.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vma_vk_funcs.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    vma::AllocatorCreateInfo create_info{};
    create_info.vulkanApiVersion = VK_API_VERSION_1_2;
    create_info.physicalDevice = physical_device_;
    create_info.device = *device_;
    create_info.instance = *instance_;
    create_info.pVulkanFunctions = &vma_vk_funcs;
    allocator_ = vma::createAllocatorUnique(create_info);
  }
}

vk::Image Context::acquireNextImage(vk::Semaphore image_available) {
  auto result = device_->acquireNextImageKHR(*swapchain_, UINT64_MAX, image_available, {},
                                             &current_swapchain_image_);
  if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    recreateSwapchain();
  else if (result != vk::Result::eSuccess)
    throw std::runtime_error("Failed to acquire next image");
  return swapchain_images_[current_swapchain_image_];
}

void Context::presentImage(vk::Semaphore render_finished) {
  auto result =
      getMainQueue().presentKHR({1, &render_finished, 1, &*swapchain_, &current_swapchain_image_});
  if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    recreateSwapchain();
  else if (result != vk::Result::eSuccess)
    throw std::runtime_error("Failed to present image");
}

void Context::recreateSwapchain() {
  auto current_extent = window_->getFramebufferSize();
  swapchain_extent_ =
      vk::Extent2D{std::clamp(current_extent.x, surface_capabilities_.minImageExtent.width,
                              surface_capabilities_.maxImageExtent.width),
                   std::clamp(current_extent.y, surface_capabilities_.minImageExtent.height,
                              surface_capabilities_.maxImageExtent.height)};
  swapchain_ = device_->createSwapchainKHRUnique({{},
                                                  *surface_,
                                                  num_swapchain_images_,
                                                  surface_format_.format,
                                                  surface_format_.colorSpace,
                                                  swapchain_extent_,
                                                  1,
                                                  vk::ImageUsageFlagBits::eColorAttachment,
                                                  vk::SharingMode::eExclusive,
                                                  {},
                                                  vk::SurfaceTransformFlagBitsKHR::eIdentity,
                                                  vk::CompositeAlphaFlagBitsKHR::eOpaque,
                                                  vk::PresentModeKHR::eFifo,
                                                  true,
                                                  *swapchain_});
}

void Context::savePipelineCache() const {
  spdlog::info("[gfx] Saving shader cache on disk");
  std::vector<uint8_t> data{device_->getPipelineCacheData(*pipeline_cache_)};
  std::ofstream f(cache_path, std::ios::out | std::ios::binary);
  std::copy(data.begin(), data.end(), std::ostreambuf_iterator<char>(f));
}
} // namespace gfx
