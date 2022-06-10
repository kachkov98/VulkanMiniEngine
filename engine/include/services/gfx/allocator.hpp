#ifndef ALLOCATOR_HPP
#define ALLOCATOR_HPP

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.hpp>

namespace vma {

template <typename OwnerType> class ObjectDestroy {
public:
  ObjectDestroy() = default;
  ObjectDestroy(OwnerType owner) noexcept : owner_(owner) {}
  OwnerType getOwner() const noexcept { return owner_; }

protected:
  template <typename T> void destroy(T t) noexcept { owner_.destroy(t); }

private:
  OwnerType owner_ = {};
};

class NoParent;

template <> class ObjectDestroy<NoParent> {
public:
  ObjectDestroy() = default;

protected:
  template <typename T> void destroy(T t) noexcept { t.destroy(); }
};

template <typename Type, typename OwnerType = NoParent>
class UniqueHandle : public ObjectDestroy<OwnerType> {
private:
  using Deleter = ObjectDestroy<OwnerType>;

public:
  UniqueHandle() = default;
  explicit UniqueHandle(const Type &value, const Deleter &deleter = Deleter()) noexcept
      : Deleter(deleter), value_(value) {}
  UniqueHandle(const UniqueHandle &) = delete;
  UniqueHandle(UniqueHandle &&rhs) noexcept
      : Deleter(std::move(static_cast<Deleter &>(rhs))), value_(rhs.release()) {}
  ~UniqueHandle() noexcept {
    if (value_)
      this->destroy(value_);
  }
  UniqueHandle &operator=(const UniqueHandle &) = delete;
  UniqueHandle &operator=(UniqueHandle &&rhs) noexcept {
    reset(rhs.release());
    static_cast<Deleter &>(*this) = std::move(static_cast<Deleter &>(rhs));
    return *this;
  }

  explicit operator bool() const noexcept { return value_; }
  const Type *operator->() const noexcept { return &value_; }
  Type *operator->() noexcept { return &value_; }
  const Type &operator*() const noexcept { return value_; }
  Type &operator*() noexcept { return value_; }
  const Type &get() const noexcept { return value_; }
  Type &get() noexcept { return value_; }

  void reset(const Type &value = Type()) noexcept {
    if (value_ == value)
      return;
    if (value_)
      this->destroy(value_);
    value_ = value;
  }

  Type release() noexcept {
    Type value = value_;
    value_ = nullptr;
    return value;
  }

private:
  Type value_;
};

using AllocationCreateInfo = VmaAllocationCreateInfo;
using PoolCreateInfo = VmaPoolCreateInfo;
using AllocatorCreateInfo = VmaAllocatorCreateInfo;

class Allocator;

// VmaAllocation wrapper
class Allocation {
public:
  using CType = VmaAllocation;
  using NativeType = VmaAllocation;

  Allocation() = default;
  Allocation(std::nullptr_t) noexcept {}
  Allocation(VmaAllocation allocation) noexcept : allocation_(allocation) {}

  Allocation &operator=(VmaAllocation allocation) noexcept {
    allocation_ = allocation;
    return *this;
  }

  Allocation &operator=(std::nullptr_t) noexcept {
    allocation_ = {};
    return *this;
  }

  operator VmaAllocation() const noexcept { return allocation_; }
  explicit operator bool() const noexcept { return allocation_ != VK_NULL_HANDLE; }
  bool operator!() const noexcept { return allocation_ == VK_NULL_HANDLE; }

private:
  VmaAllocation allocation_ = {};
};

using UniqueAllocation = UniqueHandle<Allocation, Allocator>;

// Wrapper for buffer with allocated memory
class Buffer {
public:
  Buffer() = default;
  Buffer(std::nullptr_t) noexcept {}
  Buffer(vk::Buffer buffer, Allocation allocation) noexcept
      : buffer_(buffer), allocation_(allocation) {}

  Buffer &operator=(std::nullptr_t) noexcept {
    buffer_ = nullptr;
    allocation_ = nullptr;
    return *this;
  }

  vk::Buffer getBuffer() const noexcept { return buffer_; }
  Allocation getAllocation() const noexcept { return allocation_; }
  explicit operator bool() const noexcept { return buffer_ && allocation_; }
  bool operator!() const noexcept { return !buffer_ && !allocation_; }

private:
  vk::Buffer buffer_ = {};
  Allocation allocation_ = {};
};

using UniqueBuffer = UniqueHandle<Buffer, Allocator>;

// Wrapper for image with allocated memory
class Image {
public:
  Image() = default;
  Image(std::nullptr_t) noexcept {}
  Image(vk::Image image, Allocation allocation) noexcept : image_(image), allocation_(allocation) {}

  Image &operator=(std::nullptr_t) noexcept {
    image_ = nullptr;
    allocation_ = nullptr;
    return *this;
  }

  vk::Image getImage() const noexcept { return image_; }
  Allocation getAllocation() const noexcept { return allocation_; }
  explicit operator bool() const noexcept { return image_ && allocation_; }
  bool operator!() const noexcept { return !image_ && !allocation_; }

private:
  vk::Image image_ = {};
  Allocation allocation_ = {};
};

using UniqueImage = UniqueHandle<Image, Allocator>;

// VmaPool wrapper
class Pool {
public:
  using CType = VmaPool;
  using NativeType = VmaPool;

  Pool() = default;
  Pool(std::nullptr_t) noexcept {}
  Pool(VmaPool pool) noexcept : pool_(pool) {}

  Pool &operator=(VmaPool pool) noexcept {
    pool_ = pool;
    return *this;
  }

  Pool &operator=(std::nullptr_t) noexcept {
    pool_ = {};
    return *this;
  }

  operator VmaPool() const noexcept { return pool_; }
  explicit operator bool() const noexcept { return pool_ != VK_NULL_HANDLE; }
  bool operator!() const noexcept { return pool_ == VK_NULL_HANDLE; }

private:
  VmaPool pool_ = {};
};

using UniquePool = UniqueHandle<Pool, Allocator>;

// VmaAllocator wrapper
class Allocator {
public:
  using CType = VmaAllocator;
  using NativeType = VmaAllocator;

  Allocator() = default;
  Allocator(std::nullptr_t) noexcept {}
  Allocator(VmaAllocator allocator) noexcept : allocator_(allocator) {}

  Allocator &operator=(VmaAllocator allocator) noexcept {
    allocator_ = allocator;
    return *this;
  }

  Allocator &operator=(std::nullptr_t) noexcept {
    allocator_ = {};
    return *this;
  }

  operator VmaAllocator() const noexcept { return allocator_; }
  explicit operator bool() const noexcept { return allocator_ != VK_NULL_HANDLE; }
  bool operator!() const noexcept { return allocator_ == VK_NULL_HANDLE; }

  VmaAllocatorInfo getInfo() const noexcept;
  void setCurrentFrameIndex(uint32_t index) noexcept;

  Allocation createAllocation(const vk::MemoryRequirements &memory_requirements,
                              const AllocationCreateInfo &alloc_info);
  UniqueAllocation createAllocationUnique(const vk::MemoryRequirements &memory_requirements,
                                          const AllocationCreateInfo &alloc_info);
  void bindBufferMemory(Allocation allocation, vk::Buffer buffer);
  void bindImageMemory(Allocation allocation, vk::Image image);
  void *mapMemory(Allocation allocation);
  void unmapMemory(Allocation allocation) noexcept;
  void destroy(Allocation allocation) noexcept;

  Buffer createBuffer(vk::BufferCreateInfo &buffer_info, AllocationCreateInfo &alloc_info);
  UniqueBuffer createBufferUnique(vk::BufferCreateInfo &buffer_info,
                                  AllocationCreateInfo &alloc_info);
  void destroy(Buffer buffer) noexcept;

  vk::Buffer createAliasingBuffer(Allocation allocation, vk::BufferCreateInfo &buffer_info);
  vk::UniqueBuffer createAliasingBufferUnique(Allocation allocation,
                                              vk::BufferCreateInfo &buffer_info);

  Image createImage(vk::ImageCreateInfo &image_info, AllocationCreateInfo &alloc_info);
  UniqueImage createImageUnique(vk::ImageCreateInfo &image_info, AllocationCreateInfo &alloc_info);
  void destroy(Image image) noexcept;

  vk::Image createAliasingImage(Allocation allocation, vk::ImageCreateInfo &image_info);
  vk::UniqueImage createAliasingImageUnique(Allocation allocation, vk::ImageCreateInfo &image_info);

  Pool createPool(const PoolCreateInfo &pool_info);
  UniquePool createPoolUnique(const PoolCreateInfo &pool_info);
  void destroy(Pool pool) noexcept;

  void destroy() noexcept;

private:
  VmaAllocator allocator_ = {};
};

using UniqueAllocator = UniqueHandle<Allocator>;

Allocator createAllocator(const AllocatorCreateInfo &create_info);
UniqueAllocator createAllocatorUnique(const AllocatorCreateInfo &create_info);

} // namespace vma

#endif
