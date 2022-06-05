#include "services/gfx/context.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <Tracy.hpp>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace gfx {
static const std::filesystem::path cache_path =
    std::filesystem::current_path() / "shader_cache.bin";

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

Frame::Frame(vk::PhysicalDevice physical_device, vk::Device device, vk::Queue queue,
             uint32_t queue_family_index, vma::Allocator allocator)
    : device_(device), queue_(queue), allocator_(allocator) {
  image_available_ = device_.createSemaphoreUnique({});
  render_finished_ = device_.createSemaphoreUnique({});
  render_fence_ = device_.createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
  command_pool_ = device_.createCommandPoolUnique(
      {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index});
  command_buffer_ = std::move(
      device_.allocateCommandBuffersUnique({*command_pool_, vk::CommandBufferLevel::ePrimary, 1})
          .front());
  tracy_vk_ctx_ = UniqueTracyVkCtx(physical_device, device, queue, *command_buffer_);
  memory_pool_ = allocator_.createPoolUnique(
      {/*memory type index*/ 0, VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT, 0, 0, 0, 1.0f, 0, nullptr});
}

void Frame::submit() const {
  vk::PipelineStageFlags wait_stage_mask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  vk::SubmitInfo submit_info{*image_available_, wait_stage_mask, *command_buffer_,
                             *render_finished_};
  queue_.submit(submit_info, *render_fence_);
}

void Frame::reset() const {
  if (device_.waitForFences({*render_fence_}, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout)
    throw std::runtime_error("Unexpected fence timeout");
  device_.resetFences({*render_fence_});
  command_buffer_->reset({});
}

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
    device_ = physical_device_.createDeviceUnique(vk::StructureChain{
        vk::DeviceCreateInfo{{}, queue_create_info, {}, extensions},
        vk::PhysicalDeviceVulkan11Features{}.setShaderDrawParameters(true),
        vk::PhysicalDeviceVulkan12Features{}.setDrawIndirectCount(true),
        vk::PhysicalDeviceVulkan13Features{}.setSynchronization2(true).setDynamicRendering(true)}
                                                      .get());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device_);
  }
  // Create swapchain
  recreateSwapchain();
  // Load pipeline cache
  {
    spdlog::info("[gfx] Loading shader cache from {}", cache_path.string());
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
  // Create in-flight frames
  {
    for (auto &frame : frames_)
      frame = std::move(
          Frame(physical_device_, *device_, getMainQueue(), main_queue_family_index_, *allocator_));
  }
}

bool Context::acquireNextImage(vk::Semaphore image_available) {
  ZoneScopedN("acquireNextImage");
  return checkSwapchainResult(device_->acquireNextImageKHR(*swapchain_, UINT64_MAX, image_available,
                                                           {}, &current_swapchain_image_));
}

bool Context::presentImage(vk::Semaphore render_finished) {
  ZoneScopedN("presentImage");
  auto present_info =
      vk::PresentInfoKHR{1, &render_finished, 1, &*swapchain_, &current_swapchain_image_};
  return checkSwapchainResult(getMainQueue().presentKHR(&present_info));
}

void Context::savePipelineCache() const {
  spdlog::info("[gfx] Saving shader cache to {}", cache_path.string());
  std::vector<uint8_t> data{device_->getPipelineCacheData(*pipeline_cache_)};
  std::ofstream f(cache_path, std::ios::out | std::ios::binary);
  std::copy(data.begin(), data.end(), std::ostreambuf_iterator<char>(f));
}

void Context::recreateSwapchain() {
  auto surface_capabilities = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
  num_swapchain_images_ = std::max(num_swapchain_images_, surface_capabilities.minImageCount);
  if (surface_capabilities.maxImageCount)
    num_swapchain_images_ = std::min(num_swapchain_images_, surface_capabilities.maxImageCount);
  auto current_extent = window_->getFramebufferSize();
  swapchain_extent_ =
      vk::Extent2D{std::clamp(current_extent.x, surface_capabilities.minImageExtent.width,
                              surface_capabilities.maxImageExtent.width),
                   std::clamp(current_extent.y, surface_capabilities.minImageExtent.height,
                              surface_capabilities.maxImageExtent.height)};
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
  swapchain_images_ = device_->getSwapchainImagesKHR(*swapchain_);
  swapchain_image_views_.clear();
  for (auto image : swapchain_images_) {
    vk::ImageViewCreateInfo image_view_create_info(
        vk::ImageViewCreateFlags{}, image, vk::ImageViewType::e2D, surface_format_.format,
        vk::ComponentMapping{},
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    swapchain_image_views_.push_back(device_->createImageViewUnique(image_view_create_info));
  }
  spdlog::info("[gfx] Swapchain extent: {}x{}", swapchain_extent_.width, swapchain_extent_.height);
}

bool Context::checkSwapchainResult(vk::Result result) {
  switch (result) {
  case vk::Result::eSuccess:
    return true;
  case vk::Result::eErrorOutOfDateKHR:
  case vk::Result::eSuboptimalKHR:
    waitIdle();
    recreateSwapchain();
    return false;
  default:
    vk::throwResultException(result, "checkSwapchainResult");
  }
}
} // namespace gfx
