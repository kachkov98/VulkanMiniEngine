#define VMA_IMPLEMENTATION
#include "allocator.hpp"

#define VMA_CHECK(func, ...)                                                                       \
  do {                                                                                             \
    VkResult result = func(__VA_ARGS__);                                                           \
    if (result)                                                                                    \
      vk::throwResultException(static_cast<vk::Result>(result), #func);                            \
  } while (0)

namespace vma {
Allocator createAllocator(const AllocatorCreateInfo &create_info) {
  VmaAllocator allocator;
  VMA_CHECK(vmaCreateAllocator, &create_info, &allocator);
  return allocator;
}

UniqueAllocator createAllocatorUnique(const AllocatorCreateInfo &create_info) {
  return UniqueAllocator(createAllocator(create_info), ObjectDestroy<NoParent>());
}

void Allocator::destroy() noexcept { vmaDestroyAllocator(*this); }

Allocation Allocator::createAllocation(const vk::MemoryRequirements &memory_requirements,
                                       const AllocationCreateInfo &alloc_info) {
  VmaAllocation allocation;
  VMA_CHECK(vmaAllocateMemory, *this,
            reinterpret_cast<const VkMemoryRequirements *>(&memory_requirements), &alloc_info,
            &allocation, nullptr);
  return allocation;
}

UniqueAllocation
Allocator::createAllocationUnique(const vk::MemoryRequirements &memory_requirements,
                                  const AllocationCreateInfo &alloc_info) {
  return UniqueAllocation(createAllocation(memory_requirements, alloc_info),
                          ObjectDestroy<Allocator>(*this));
}

void Allocator::bindBufferMemory(Allocation allocation, vk::Buffer buffer) {
  VMA_CHECK(vmaBindBufferMemory, *this, allocation, buffer);
}

void Allocator::bindImageMemory(Allocation allocation, vk::Image image) {
  VMA_CHECK(vmaBindImageMemory, *this, allocation, image);
}

void *Allocator::mapMemory(Allocation allocation) {
  void *memory;
  VMA_CHECK(vmaMapMemory, *this, allocation, &memory);
  return memory;
}

void Allocator::unmapMemory(Allocation allocation) noexcept { vmaUnmapMemory(*this, allocation); }

void Allocator::destroy(Allocation allocation) noexcept { vmaFreeMemory(*this, allocation); };

Buffer Allocator::createBuffer(vk::BufferCreateInfo &buffer_info,
                               AllocationCreateInfo &alloc_info) {
  VkBuffer buffer;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateBuffer, *this, reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
            &alloc_info, &buffer, &allocation, nullptr);
  return Buffer(buffer, allocation);
}
UniqueBuffer Allocator::createBufferUnique(vk::BufferCreateInfo &buffer_info,
                                           AllocationCreateInfo &alloc_info) {
  return UniqueBuffer(createBuffer(buffer_info, alloc_info), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Buffer buffer) noexcept {
  vmaDestroyBuffer(*this, buffer.getBuffer(), buffer.getAllocation());
}

Image Allocator::createImage(vk::ImageCreateInfo &image_info, AllocationCreateInfo &alloc_info) {
  VkImage image;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateImage, *this, reinterpret_cast<const VkImageCreateInfo *>(&image_info),
            &alloc_info, &image, &allocation, nullptr);
  return Image(image, allocation);
}

UniqueImage Allocator::createImageUnique(vk::ImageCreateInfo &image_info,
                                         AllocationCreateInfo &alloc_info) {
  return UniqueImage(createImage(image_info, alloc_info), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Image image) noexcept {
  vmaDestroyImage(*this, image.getImage(), image.getAllocation());
}

Pool Allocator::createPool(const PoolCreateInfo &pool_info) {
  VmaPool pool;
  VMA_CHECK(vmaCreatePool, *this, &pool_info, &pool);
  return pool;
}

UniquePool Allocator::createPoolUnique(const PoolCreateInfo &pool_info) {
  return UniquePool(createPool(pool_info), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Pool pool) noexcept { vmaDestroyPool(*this, pool); };
} // namespace vma