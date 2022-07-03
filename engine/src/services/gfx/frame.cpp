#include "services/gfx/frame.hpp"

namespace gfx {
TransientAllocator::TransientAllocator(vma::Allocator allocator) : allocator_(allocator) {
  vma::AllocationCreateInfo alloc_info{};
  alloc_info.requiredFlags =
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
  alloc_info.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  vma::PoolCreateInfo pool_info{};
  pool_info.memoryTypeIndex = allocator_.findMemoryTypeIndex(UINT32_MAX, alloc_info);
  pool_info.flags =
      VMA_POOL_CREATE_LINEAR_ALGORITHM_BIT | VMA_POOL_CREATE_IGNORE_BUFFER_IMAGE_GRANULARITY_BIT;
  pool_ = allocator_.createPoolUnique(pool_info);
}

std::pair<vk::Buffer, void *> TransientAllocator::createBuffer(vk::BufferUsageFlags usage,
                                                               size_t size) {
  vma::AllocationCreateInfo allocation_info{};
  allocation_info.flags =
      VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
  allocation_info.pool = *pool_;
  buffers_.push_back(allocator_.createBufferUnique({{}, size, usage}, allocation_info));
  const auto &buffer = buffers_.back();
  return {buffer->getBuffer(), allocator_.getAllocationInfo(buffer->getAllocation()).pMappedData};
}

Frame::Frame(vk::PhysicalDevice physical_device, vk::Device device, uint32_t queue_family_index,
             uint32_t queue_index, vma::Allocator allocator)
    : device_(device), queue_(device.getQueue(queue_family_index, queue_index)),
      transient_allocator_(allocator) {
  image_available_ = device_.createSemaphoreUnique({});
  render_finished_ = device_.createSemaphoreUnique({});
  render_fence_ = device_.createFenceUnique({vk::FenceCreateFlagBits::eSignaled});
  command_pool_ = device_.createCommandPoolUnique(
      {vk::CommandPoolCreateFlagBits::eTransient, queue_family_index});
  command_buffer_ = std::move(
      device_.allocateCommandBuffersUnique({*command_pool_, vk::CommandBufferLevel::ePrimary, 1})
          .front());
  tracy_vk_ctx_ = UniqueTracyVkCtx(physical_device, device, queue_, *command_buffer_);
}

void Frame::submit() const {
  ZoneScoped;
  vk::PipelineStageFlags wait_stage_mask = vk::PipelineStageFlagBits::eAllCommands;
  queue_.submit(
      vk::SubmitInfo{*image_available_, wait_stage_mask, *command_buffer_, *render_finished_},
      *render_fence_);
}

void Frame::reset() {
  ZoneScoped;
  if (device_.waitForFences(*render_fence_, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout)
    throw std::runtime_error("Unexpected render fence timeout");
  device_.resetFences({*render_fence_});
  device_.resetCommandPool(*command_pool_);
  transient_allocator_.reset();
}
} // namespace gfx
