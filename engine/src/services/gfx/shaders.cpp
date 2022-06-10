#include "services/gfx/shaders.hpp"

#include <spdlog/spdlog.h>

#include <fstream>
#include <stdexcept>

#define SPV_CHECK(result)                                                                          \
  do {                                                                                             \
    if ((result) != SPV_REFLECT_RESULT_SUCCESS)                                                    \
      throw std::runtime_error("spirv-reflect runtime error");                                     \
  } while (0)

namespace gfx {
static const std::filesystem::path shader_path = std::filesystem::current_path() / "../shaders";

ShaderModule::ShaderModule(vk::Device device, const ShaderCode &code)
    : shader_module_(device.createShaderModuleUnique({{}, code})), reflection_(code) {
  SPV_CHECK(reflection_.GetResult());
}

static std::vector<vk::DescriptorSetLayoutBinding>
getDescriptorSetLayoutBindings(const SpvReflectDescriptorSet *set, vk::ShaderStageFlagBits stage) {
  std::vector<const SpvReflectDescriptorBinding *> descriptor_bindings(
      set->bindings, set->bindings + set->binding_count);
  std::vector<vk::DescriptorSetLayoutBinding> res;
  std::transform(descriptor_bindings.begin(), descriptor_bindings.end(), std::back_inserter(res),
                 [&](const SpvReflectDescriptorBinding *binding) {
                   return vk::DescriptorSetLayoutBinding(
                       binding->binding, static_cast<vk::DescriptorType>(binding->descriptor_type),
                       binding->count, stage);
                 });
  return res;
}

DescriptorSetLayouts ShaderModule::getDescriptorSetLayouts() const {
  uint32_t count = 0;
  SPV_CHECK(reflection_.EnumerateDescriptorSets(&count, nullptr));
  std::vector<SpvReflectDescriptorSet *> descriptor_sets(count);
  SPV_CHECK(reflection_.EnumerateDescriptorSets(&count, descriptor_sets.data()));
  auto stage = getStage();
  DescriptorSetLayouts res;
  std::transform(descriptor_sets.begin(), descriptor_sets.end(), std::back_inserter(res),
                 [&](const SpvReflectDescriptorSet *set) {
                   return std::make_pair(set->set, getDescriptorSetLayoutBindings(set, stage));
                 });
  return res;
}

PushConstantRanges ShaderModule::getPushConstantRanges() const {
  uint32_t count = 0;
  SPV_CHECK(reflection_.EnumeratePushConstantBlocks(&count, nullptr));
  std::vector<SpvReflectBlockVariable *> push_constant_blocks(count);
  SPV_CHECK(reflection_.EnumeratePushConstantBlocks(&count, push_constant_blocks.data()));
  auto stage = getStage();
  PushConstantRanges res;
  std::transform(push_constant_blocks.begin(), push_constant_blocks.end(), std::back_inserter(res),
                 [&](const SpvReflectBlockVariable *var) {
                   return std::make_pair(std::string(var->name),
                                         vk::PushConstantRange(stage, var->offset, var->size));
                 });
  return res;
};

ShaderModule ShaderModuleCache::create(const std::filesystem::path &rel_path) {
  std::filesystem::path path = shader_path / rel_path;
  spdlog::info("[gfx] Loading shader module from {}", path.string());
  std::ifstream f(path, std::ios::in | std::ios::binary);
  std::vector<char> code{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  return ShaderModule(device_, ShaderCode(reinterpret_cast<uint32_t *>(code.data()),
                                          reinterpret_cast<uint32_t *>(code.data() + code.size())));
}
} // namespace gfx
