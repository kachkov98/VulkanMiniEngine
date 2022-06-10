#ifndef SHADERS_HPP
#define SHADERS_HPP

#include "common/cache.hpp"

#include <spirv_reflect.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <filesystem>
#include <string>
#include <vector>

namespace gfx {
using ShaderCode = std::vector<uint32_t>;

template <typename Key, typename Value> using KeyValueVector = std::vector<std::pair<Key, Value>>;

using DescriptorSetLayouts = KeyValueVector<uint32_t, std::vector<vk::DescriptorSetLayoutBinding>>;
using PushConstantRanges = KeyValueVector<std::string, vk::PushConstantRange>;

class ShaderModule {
public:
  ShaderModule() = default;
  ShaderModule(vk::Device device, const ShaderCode &code);

  vk::ShaderModule get() const noexcept { return *shader_module_; }

  const char *getName() const noexcept { return reflection_.GetEntryPointName(); };
  vk::ShaderStageFlagBits getStage() const noexcept {
    return static_cast<vk::ShaderStageFlagBits>(reflection_.GetShaderStage());
  };

  DescriptorSetLayouts getDescriptorSetLayouts() const;
  PushConstantRanges getPushConstantRanges() const;

private:
  vk::UniqueShaderModule shader_module_;
  spv_reflect::ShaderModule reflection_;
};

class ShaderModuleCache final
    : public vme::Cache<ShaderModuleCache, std::filesystem::path, ShaderModule> {
public:
  ShaderModuleCache(vk::Device device = {}) : device_(device) {}

  ShaderModule create(const std::filesystem::path &rel_path);

private:
  vk::Device device_;
};
} // namespace gfx

#endif
