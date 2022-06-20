#include "services/gfx/context.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <Tracy.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <stdexcept>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace gfx {

static std::vector<const char *> getInstanceExtensions() {
  uint32_t count;
  const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
  std::vector<const char *> extensions(glfw_extensions, glfw_extensions + count);
#ifndef NDEBUG
  extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
  return extensions;
}

static std::vector<const char *> getRequiredDeviceExtensions() {
  return {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
}

static std::vector<const char *> getDesiredDeviceExtensions() {
  return {VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME, VK_EXT_MEMORY_BUDGET_EXTENSION_NAME};
}

static std::vector<const char *> getValidationLayers() {
  return {
#ifndef NDEBUG
      "VK_LAYER_KHRONOS_validation"
#endif
  };
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
             uint32_t queue_family_index)
    : device_(device), queue_(queue) {
  image_available_ = device_.createSemaphoreUnique({});
  render_finished_ = device_.createSemaphoreUnique({});
  render_fence_ = device_.createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
  command_pool_ = device_.createCommandPoolUnique(
      {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index});
  command_buffer_ = std::move(
      device_.allocateCommandBuffersUnique({*command_pool_, vk::CommandBufferLevel::ePrimary, 1})
          .front());
  tracy_vk_ctx_ = UniqueTracyVkCtx(physical_device, device, queue, *command_buffer_);
}

void Frame::submit() const {
  vk::PipelineStageFlags wait_stage_mask = vk::PipelineStageFlagBits::eAllCommands;
  queue_.submit(
      vk::SubmitInfo{*image_available_, wait_stage_mask, *command_buffer_, *render_finished_},
      *render_fence_);
}

void Frame::reset() const {
  if (device_.waitForFences(*render_fence_, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout)
    throw std::runtime_error("Unexpected render fence timeout");
  device_.resetFences({*render_fence_});
  device_.resetCommandPool(*command_pool_);
}

Context::Context(const wsi::Window &window) {
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
      spdlog::info("[gfx]    {}", physical_device.getProperties().deviceName);
    physical_device_ = physical_devices[0];
    spdlog::info("[gfx] Selected device {}", physical_device_.getProperties().deviceName);
    auto available_extensions = physical_device_.enumerateDeviceExtensionProperties();
    auto extension_supported = [&available_extensions](std::string_view extension) {
      return std::find_if(available_extensions.begin(), available_extensions.end(),
                          [extension](const vk::ExtensionProperties &properties) {
                            return properties.extensionName == extension;
                          }) != available_extensions.end();
    };
    for (auto required_extension : getRequiredDeviceExtensions()) {
      if (!extension_supported(required_extension))
        throw std::runtime_error("Required device extension not supported");
      enabled_extensions_.push_back(required_extension);
    }
    for (auto desired_extension : getDesiredDeviceExtensions())
      if (extension_supported(desired_extension))
        enabled_extensions_.push_back(desired_extension);
    spdlog::info("[gfx] Enabled extensions:");
    for (auto extension : enabled_extensions_)
      spdlog::info("[gfx]    {}", extension);
  }
  // Create surface
  {
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(*instance_, window.getHandle(), nullptr, &surface) != VK_SUCCESS)
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
    device_ = physical_device_.createDeviceUnique(vk::StructureChain{
        vk::DeviceCreateInfo{{}, queue_create_info, {}, enabled_extensions_},
        vk::PhysicalDeviceVulkan11Features{}.setShaderDrawParameters(true),
        vk::PhysicalDeviceVulkan12Features{}.setDrawIndirectCount(true),
        vk::PhysicalDeviceVulkan13Features{}.setSynchronization2(true).setDynamicRendering(true)}
                                                      .get());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*device_);
  }
  // Create swapchain
  recreateSwapchain(window.getFramebufferSize());
  // Create resource caches
  descriptor_set_layout_cache_ = DescriptorSetLayoutCache(*device_);
  shader_module_cache_ = ShaderModuleCache(*device_);
  pipeline_layout_cache_ = PipelineLayoutCache(*device_);
  pipeline_cache_ = PipelineCache(*device_);
  // Create allocator
  {
    VmaVulkanFunctions vma_vk_funcs{};
    vma_vk_funcs.vkGetInstanceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetInstanceProcAddr;
    vma_vk_funcs.vkGetDeviceProcAddr = VULKAN_HPP_DEFAULT_DISPATCHER.vkGetDeviceProcAddr;

    vma::AllocatorCreateInfo create_info{};
    create_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    if (isExtensionEnabled(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
      create_info.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    create_info.vulkanApiVersion = VK_API_VERSION_1_3;
    create_info.physicalDevice = physical_device_;
    create_info.device = *device_;
    create_info.instance = *instance_;
    create_info.pVulkanFunctions = &vma_vk_funcs;
    allocator_ = vma::createAllocatorUnique(create_info);
    allocator_->setCurrentFrameIndex(current_frame_);
  }
  // Create staging buffer
  staging_buffer_ =
      std::move(StagingBuffer(*device_, getMainQueue(), main_queue_family_index_, *allocator_));
  // Create in-flight frames
  for (auto &frame : frames_)
    frame = Frame(physical_device_, *device_, getMainQueue(), main_queue_family_index_);
}

void Context::recreateSwapchain(glm::uvec2 new_extent) {
  if (!new_extent.x || !new_extent.y)
    return;
  auto surface_capabilities = physical_device_.getSurfaceCapabilitiesKHR(*surface_);
  swapchain_extent_ =
      vk::Extent2D{std::clamp(new_extent.x, surface_capabilities.minImageExtent.width,
                              surface_capabilities.maxImageExtent.width),
                   std::clamp(new_extent.y, surface_capabilities.minImageExtent.height,
                              surface_capabilities.maxImageExtent.height)};
  num_swapchain_images_ = std::max(num_swapchain_images_, surface_capabilities.minImageCount);
  if (surface_capabilities.maxImageCount)
    num_swapchain_images_ = std::min(num_swapchain_images_, surface_capabilities.maxImageCount);
  auto present_modes = physical_device_.getSurfacePresentModesKHR(*surface_);
  auto present_mode = vk::PresentModeKHR::eFifo;
  if (auto it = std::find(present_modes.begin(), present_modes.end(), vk::PresentModeKHR::eMailbox);
      it != present_modes.end())
    present_mode = *it;
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
                                                  present_mode,
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

bool Context::isExtensionEnabled(std::string_view name) const noexcept {
  return std::find(enabled_extensions_.begin(), enabled_extensions_.end(), name) !=
         enabled_extensions_.end();
}

void Context::acquireNextImage(vk::Semaphore image_available) {
  ZoneScopedN("acquireNextImage");
  auto [res, index] = device_->acquireNextImageKHR(*swapchain_, UINT64_MAX, image_available, {});
  current_swapchain_image_ = index;
  if (res != vk::Result::eSuccess)
    spdlog::warn("[gfx] acquireNextImage - {}", vk::to_string(res));
}

void Context::presentImage(vk::Semaphore render_finished) {
  ZoneScopedN("presentImage");
  auto res = getMainQueue().presentKHR({render_finished, *swapchain_, current_swapchain_image_});
  if (res != vk::Result::eSuccess)
    spdlog::warn("[gfx] presentImage - {}", vk::to_string(res));
}
} // namespace gfx
