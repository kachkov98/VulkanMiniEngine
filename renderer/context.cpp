#include "context.hpp"
#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

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

static std::vector<const char *> getDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
          VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME};
}

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

Context::Context(const wsi::Window *window) : window_(window) {
  // Create instance
  {
    VULKAN_HPP_DEFAULT_DISPATCHER.init(glfwGetInstanceProcAddress);
    vk::ApplicationInfo app_info{"VulkanMiniEngine", VK_MAKE_VERSION(0, 0, 1), "VulkanMiniEngine",
                                 VK_MAKE_VERSION(0, 0, 1), VK_API_VERSION_1_2};
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
  // Create surface
  {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*instance_, window_->getHandle(), nullptr, &surface) != VK_SUCCESS)
      throw std::runtime_error("glfwCreateWindowSurface failed");
    surface_ = vk::UniqueSurfaceKHR(surface, *instance_);
  }
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
    vk::SurfaceCapabilitiesKHR capabilities = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
    std::vector<vk::SurfaceFormatKHR> formats = physical_device_.getSurfaceFormatsKHR(*surface_);
    std::vector<vk::PresentModeKHR> present_modes =
        physical_device_.getSurfacePresentModesKHR(*surface_);
    //swapchain_ = device_->createSwapchainKHRUnique({{}, *surface_, });
  }
  // Create allocator
  {
    const auto &dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER;
    VmaVulkanFunctions vma_vk_funcs{};
    vma_vk_funcs.vkGetPhysicalDeviceProperties = dispatcher.vkGetPhysicalDeviceProperties;
    vma_vk_funcs.vkGetPhysicalDeviceMemoryProperties = 
        dispatcher.vkGetPhysicalDeviceMemoryProperties;
    vma_vk_funcs.vkGetPhysicalDeviceMemoryProperties2KHR =
        dispatcher.vkGetPhysicalDeviceMemoryProperties2;
    vma_vk_funcs.vkAllocateMemory = dispatcher.vkAllocateMemory;
    vma_vk_funcs.vkFreeMemory = dispatcher.vkFreeMemory;
    vma_vk_funcs.vkMapMemory = dispatcher.vkMapMemory;
    vma_vk_funcs.vkUnmapMemory = dispatcher.vkUnmapMemory;
    vma_vk_funcs.vkFlushMappedMemoryRanges = dispatcher.vkFlushMappedMemoryRanges;
    vma_vk_funcs.vkInvalidateMappedMemoryRanges = dispatcher.vkInvalidateMappedMemoryRanges;
    vma_vk_funcs.vkBindBufferMemory = dispatcher.vkBindBufferMemory;
    vma_vk_funcs.vkBindBufferMemory2KHR = dispatcher.vkBindBufferMemory2;
    vma_vk_funcs.vkBindImageMemory = dispatcher.vkBindImageMemory;
    vma_vk_funcs.vkBindImageMemory2KHR = dispatcher.vkBindImageMemory2;
    vma_vk_funcs.vkGetBufferMemoryRequirements = dispatcher.vkGetBufferMemoryRequirements;
    vma_vk_funcs.vkGetBufferMemoryRequirements2KHR = dispatcher.vkGetBufferMemoryRequirements2;
    vma_vk_funcs.vkGetImageMemoryRequirements = dispatcher.vkGetImageMemoryRequirements;
    vma_vk_funcs.vkGetImageMemoryRequirements2KHR = dispatcher.vkGetImageMemoryRequirements2;
    vma_vk_funcs.vkCreateBuffer = dispatcher.vkCreateBuffer;
    vma_vk_funcs.vkDestroyBuffer = dispatcher.vkDestroyBuffer;
    vma_vk_funcs.vkCreateImage = dispatcher.vkCreateImage;
    vma_vk_funcs.vkDestroyImage = dispatcher.vkDestroyImage;
    vma_vk_funcs.vkCmdCopyBuffer = dispatcher.vkCmdCopyBuffer;

    vma::AllocatorCreateInfo create_info{};
    create_info.vulkanApiVersion = VK_API_VERSION_1_2;
    create_info.physicalDevice = physical_device_;
    create_info.device = *device_;
    create_info.instance = *instance_;
    create_info.pVulkanFunctions = &vma_vk_funcs;
    allocator_ = vma::createAllocatorUnique(create_info);
  }
}
} // namespace gfx