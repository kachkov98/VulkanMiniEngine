#define VMA_IMPLEMENTATION
#include "services/gfx/allocator.hpp"

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

VmaAllocatorInfo Allocator::getInfo() const noexcept {
  VmaAllocatorInfo allocator_info;
  vmaGetAllocatorInfo(*this, &allocator_info);
  return allocator_info;
}

void Allocator::setCurrentFrameIndex(uint32_t index) noexcept {
  vmaSetCurrentFrameIndex(*this, index);
}

uint32_t Allocator::findMemoryTypeIndex(uint32_t memory_type_bits,
                                        const AllocationCreateInfo &alloc_info) {
  uint32_t memory_type_index;
  VMA_CHECK(vmaFindMemoryTypeIndex, *this, memory_type_bits, &alloc_info, &memory_type_index);
  return memory_type_index;
}

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

VmaAllocationInfo Allocator::getAllocationInfo(Allocation allocation) const noexcept {
  VmaAllocationInfo allocation_info;
  vmaGetAllocationInfo(*this, allocation, &allocation_info);
  return allocation_info;
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

Buffer Allocator::createBuffer(const vk::BufferCreateInfo &buffer_info,
                               const AllocationCreateInfo &alloc_info) {
  VkBuffer buffer;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateBuffer, *this, reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info),
            &alloc_info, &buffer, &allocation, nullptr);
  return Buffer(buffer, allocation);
}
UniqueBuffer Allocator::createBufferUnique(const vk::BufferCreateInfo &buffer_info,
                                           const AllocationCreateInfo &alloc_info) {
  return UniqueBuffer(createBuffer(buffer_info, alloc_info), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Buffer buffer) noexcept {
  vmaDestroyBuffer(*this, buffer.getBuffer(), buffer.getAllocation());
}

vk::Buffer Allocator::createAliasingBuffer(Allocation allocation,
                                           const vk::BufferCreateInfo &buffer_info) {
  VkBuffer buffer;
  VMA_CHECK(vmaCreateAliasingBuffer, *this, allocation,
            reinterpret_cast<const VkBufferCreateInfo *>(&buffer_info), &buffer);
  return buffer;
}

vk::UniqueBuffer Allocator::createAliasingBufferUnique(Allocation allocation,
                                                       const vk::BufferCreateInfo &buffer_info) {
  return vk::UniqueBuffer(
      createAliasingBuffer(allocation, buffer_info),
      vk::ObjectDestroy<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(getInfo().device));
}

Image Allocator::createImage(const vk::ImageCreateInfo &image_info,
                             const AllocationCreateInfo &alloc_info) {
  VkImage image;
  VmaAllocation allocation;
  VMA_CHECK(vmaCreateImage, *this, reinterpret_cast<const VkImageCreateInfo *>(&image_info),
            &alloc_info, &image, &allocation, nullptr);
  return Image(image, allocation);
}

UniqueImage Allocator::createImageUnique(const vk::ImageCreateInfo &image_info,
                                         const AllocationCreateInfo &alloc_info) {
  return UniqueImage(createImage(image_info, alloc_info), ObjectDestroy<Allocator>(*this));
}

void Allocator::destroy(Image image) noexcept {
  vmaDestroyImage(*this, image.getImage(), image.getAllocation());
}

vk::Image Allocator::createAliasingImage(Allocation allocation,
                                         const vk::ImageCreateInfo &image_info) {
  VkImage image;
  VMA_CHECK(vmaCreateAliasingImage, *this, allocation,
            reinterpret_cast<const VkImageCreateInfo *>(&image_info), &image);
  return image;
}

vk::UniqueImage Allocator::createAliasingImageUnique(Allocation allocation,
                                                     const vk::ImageCreateInfo &image_info) {
  return vk::UniqueImage(
      createAliasingImage(allocation, image_info),
      vk::ObjectDestroy<vk::Device, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>(getInfo().device));
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
