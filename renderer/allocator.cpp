#define VMA_IMPLEMENTATION
#include "allocator.hpp"

#define VMA_CHECK(func, message)                                                                   \
  do {                                                                                             \
    VkResult result = (func);                                                                      \
    if (result)                                                                                    \
      vk::throwResultException(static_cast<vk::Result>(result), message);                          \
  } while (0)

namespace vma {
#if 0
void UniqueAllocation::bindBufferMemory(vk::Buffer buffer) {
  getOwner().bindBufferMemory(get(), buffer);
}

void UniqueAllocation::bindImageMemory(vk::Image image) {
  getOwner().bindImageMemory(get(), image);
}

void *UniqueAllocation::mapMemory() { return getOwner().mapMemory(get()); };

void UniqueAllocation::unmapMemory() noexcept { getOwner().unmapMemory(get()); };
#endif
Allocator createAllocator(const AllocatorCreateInfo &createInfo) {
  VmaAllocator allocator;
  VMA_CHECK(vmaCreateAllocator(&createInfo, &allocator), "vmaCreateAllocator");
  return allocator;
}

UniqueAllocator createAllocatorUnique(const AllocatorCreateInfo &createInfo) {
  return UniqueAllocator(createAllocator(createInfo), ObjectDestroy<NoParent>());
}

void Allocator::destroy() noexcept { vmaDestroyAllocator(*this); }

Allocation Allocator::createAllocation(const vk::MemoryRequirements &memoryRequirements,
                                       const AllocationCreateInfo &allocInfo) {
  VmaAllocation allocation;
  VMA_CHECK(vmaAllocateMemory(*this, &static_cast<VkMemoryRequirements>(memoryRequirements),
                              &allocInfo, &allocation, nullptr),
            "vmaAllocateMemory");
  return allocation;
}

UniqueAllocation Allocator::createAllocationUnique(const vk::MemoryRequirements &memoryRequirements,
                                                   const AllocationCreateInfo &allocInfo) {
  return UniqueAllocation(createAllocation(memoryRequirements, allocInfo),
                          ObjectDestroy<Allocator>(*this));
}

void Allocator::bindBufferMemory(Allocation allocation, vk::Buffer buffer) {
  VMA_CHECK(vmaBindBufferMemory(*this, allocation, buffer), "vmaBindBufferMemory");
}

void Allocator::bindImageMemory(Allocation allocation, vk::Image image) {
  VMA_CHECK(vmaBindImageMemory(*this, allocation, image), "vmaBindImageMemory");
}

void *Allocator::mapMemory(Allocation allocation) {
  void *memory;
  VMA_CHECK(vmaMapMemory(*this, allocation, &memory), "vmaMapMemory");
  return memory;
}

void Allocator::unmapMemory(Allocation allocation) noexcept { vmaUnmapMemory(*this, allocation); }

void Allocator::destroy(Allocation allocation) noexcept { vmaFreeMemory(*this, allocation); };

Buffer Allocator::createBuffer(vk::BufferCreateInfo &bufferInfo, AllocationCreateInfo &allocInfo) {
  VkBuffer buffer;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateBuffer(*this, &static_cast<VkBufferCreateInfo>(bufferInfo), &allocInfo,
                            &buffer, &allocation, nullptr),
            "vmaCreateBuffer");
  return Buffer(buffer, allocation);
}
UniqueBuffer Allocator::createBufferUnique(vk::BufferCreateInfo &bufferInfo,
                                           AllocationCreateInfo &allocInfo) {
  return UniqueBuffer(createBuffer(bufferInfo, allocInfo), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Buffer buffer) noexcept {
  vmaDestroyBuffer(*this, buffer.getBuffer(), buffer.getAllocation());
}

Image Allocator::createImage(vk::ImageCreateInfo &imageInfo, AllocationCreateInfo &allocInfo) {
  VkImage image;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateImage(*this, &static_cast<VkImageCreateInfo>(imageInfo), &allocInfo, &image,
                           &allocation, nullptr),
            "vmaCreateImage");
  return Image(image, allocation);
}

UniqueImage Allocator::createImageUnique(vk::ImageCreateInfo &imageInfo,
                                         AllocationCreateInfo &allocInfo) {
  return UniqueImage(createImage(imageInfo, allocInfo), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Image image) noexcept {
  vmaDestroyImage(*this, image.getImage(), image.getAllocation());
}

Pool Allocator::createPool(const PoolCreateInfo &poolInfo) {
  VmaPool pool;
  VMA_CHECK(vmaCreatePool(*this, &poolInfo, &pool), "vmaCreatePool");
  return pool;
}

UniquePool Allocator::createPoolUnique(const PoolCreateInfo &poolInfo) {
  return UniquePool(createPool(poolInfo), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Pool pool) noexcept { vmaDestroyPool(*this, pool); };
} // namespace vma