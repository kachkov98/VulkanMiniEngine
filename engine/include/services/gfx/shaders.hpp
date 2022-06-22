#ifndef SHADERS_HPP
#define SHADERS_HPP

#include "common/cache.hpp"

#include <spirv_reflect.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <optional>
#include <string>
#include <vector>

namespace gfx {
using DescriptorSetLayoutBindings = std::vector<vk::DescriptorSetLayoutBinding>;
using DescriptorSetLayouts = std::vector<std::pair<uint32_t, DescriptorSetLayoutBindings>>;
using PushConstantRange = std::optional<vk::PushConstantRange>;

class ShaderModule final {
public:
  using Code = std::vector<uint32_t>;

  ShaderModule() = default;
  ShaderModule(vk::Device device, const Code &code);

  vk::ShaderModule get() const noexcept { return *shader_module_; }

  const char *getName() const noexcept { return reflection_.GetEntryPointName(); };
  vk::ShaderStageFlagBits getStage() const noexcept {
    return static_cast<vk::ShaderStageFlagBits>(reflection_.GetShaderStage());
  };

  DescriptorSetLayouts getDescriptorSetLayouts() const;
  PushConstantRange getPushConstantRange() const;

private:
  vk::UniqueShaderModule shader_module_;
  spv_reflect::ShaderModule reflection_;
};

class ShaderModuleCache final : public vme::Cache<ShaderModuleCache, std::string, ShaderModule> {
public:
  ShaderModuleCache(vk::Device device = {}) : device_(device) {}

  ShaderModule create(const std::string &name);

private:
  vk::Device device_;
};
} // namespace gfx

#endif