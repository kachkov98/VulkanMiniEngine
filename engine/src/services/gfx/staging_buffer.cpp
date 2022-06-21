#include "services/gfx/staging_buffer.hpp"

namespace gfx {
class CmdBufGenerator {
public:
  CmdBufGenerator(vk::CommandBuffer cmd_buf, vk::Buffer staging_buf)
      : cmd_buf_(cmd_buf), staging_buf_(staging_buf) {}

  void operator()(const StagingBuffer::BufferCopy &copy) {
    cmd_buf_.copyBuffer2(vk::CopyBufferInfo2{staging_buf_, copy.buffer, copy.regions});
  }

  void operator()(const StagingBuffer::ImageCopy &copy) {
    if (copy.old_layout != vk::ImageLayout::eTransferDstOptimal) {
      vk::ImageMemoryBarrier2 barrier{vk::PipelineStageFlagBits2::eTopOfPipe,
                                      vk::AccessFlagBits2::eNone,
                                      vk::PipelineStageFlagBits2::eCopy,
                                      vk::AccessFlagBits2::eTransferWrite,
                                      copy.old_layout,
                                      vk::ImageLayout::eTransferDstOptimal,
                                      {},
                                      {},
                                      copy.image,
                                      copy.subresource};
      cmd_buf_.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, barrier});
    }

    cmd_buf_.copyBufferToImage2(vk::CopyBufferToImageInfo2{
        staging_buf_, copy.image, vk::ImageLayout::eTransferDstOptimal, copy.regions});

    if (copy.new_layout != vk::ImageLayout::eTransferDstOptimal) {
      vk::ImageMemoryBarrier2 barrier{vk::PipelineStageFlagBits2::eCopy,
                                      vk::AccessFlagBits2::eTransferWrite,
                                      vk::PipelineStageFlagBits2::eBottomOfPipe,
                                      vk::AccessFlagBits2::eNone,
                                      vk::ImageLayout::eTransferDstOptimal,
                                      copy.new_layout,
                                      {},
                                      {},
                                      copy.image,
                                      copy.subresource};
      cmd_buf_.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, barrier});
    }
  }

private:
  vk::CommandBuffer cmd_buf_;
  vk::Buffer staging_buf_;
};

StagingBuffer::StagingBuffer(vk::Device device, vk::Queue queue, uint32_t queue_family_index,
                             vma::Allocator allocator)
    : device_(device), queue_(queue) {
  upload_fence_ = device_.createFenceUnique({});
  command_pool_ = device_.createCommandPoolUnique(
      {vk::CommandPoolCreateFlagBits::eResetCommandBuffer, queue_family_index});
  command_buffer_ = std::move(
      device_.allocateCommandBuffersUnique({*command_pool_, vk::CommandBufferLevel::ePrimary, 1})
          .front());
  staging_buffer_ = allocator.createBufferUnique(
      {{}, max_size, vk::BufferUsageFlagBits::eTransferSrc},
      {VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
       VMA_MEMORY_USAGE_AUTO});
  mapped_data_ = allocator.getAllocationInfo(staging_buffer_->getAllocation()).pMappedData;
}

void StagingBuffer::flush() {
  command_buffer_->begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  for (const auto &copy : copies_)
    std::visit(CmdBufGenerator(*command_buffer_, staging_buffer_->getBuffer()), copy);
  command_buffer_->end();
  queue_.submit(vk::SubmitInfo{{}, {}, *command_buffer_}, *upload_fence_);
  if (device_.waitForFences(*upload_fence_, VK_TRUE, UINT64_MAX) == vk::Result::eTimeout)
    throw std::runtime_error("Unexpected upload fence timeout");
  device_.resetFences(*upload_fence_);
  command_buffer_->reset({});
  offset_ = 0;
  copies_.clear();
}

size_t StagingBuffer::copyData(const void *data, size_t size) {
  if (size > max_size)
    throw std::runtime_error("Data block exceeds staging buffer size");
  else if (offset_ + size > max_size)
    flush();
  assert(offset_ + size <= max_size);
  memcpy(reinterpret_cast<void *>(reinterpret_cast<std::byte *>(mapped_data_) + offset_), data,
         size);
  auto offset = offset_;
  offset_ += size;
  return offset;
}
} // namespace gfx
