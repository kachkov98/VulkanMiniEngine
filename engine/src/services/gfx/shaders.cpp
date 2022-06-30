#include "services/gfx/shaders.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>
#include <stdexcept>

#define SPV_CHECK(result)                                                                          \
  do {                                                                                             \
    if ((result) != SPV_REFLECT_RESULT_SUCCESS)                                                    \
      throw std::runtime_error("spirv-reflect runtime error");                                     \
  } while (0)

namespace gfx {
static const std::filesystem::path shaders_path =
    std::filesystem::current_path() / ".." / "shaders";

ShaderModule::ShaderModule(vk::Device device, const Code &code)
    : shader_module_(device.createShaderModuleUnique({{}, code})), reflection_(code) {
  SPV_CHECK(reflection_.GetResult());
}

static DescriptorSetLayoutBindings
getDescriptorSetLayoutBindings(const SpvReflectDescriptorSet *set, vk::ShaderStageFlagBits stage) {
  std::vector<const SpvReflectDescriptorBinding *> descriptor_bindings(
      set->bindings, set->bindings + set->binding_count);
  DescriptorSetLayoutBindings res;
  std::transform(descriptor_bindings.begin(), descriptor_bindings.end(), std::back_inserter(res),
                 [&](const SpvReflectDescriptorBinding *binding) {
                   return vk::DescriptorSetLayoutBinding(
                       binding->binding, static_cast<vk::DescriptorType>(binding->descriptor_type),
                       binding->count, stage);
                 });
  return res;
}

std::vector<std::pair<uint32_t, DescriptorSetLayoutBindings>>
ShaderModule::getDescriptorSetLayouts() const {
  uint32_t count = 0;
  SPV_CHECK(reflection_.EnumerateDescriptorSets(&count, nullptr));
  std::vector<SpvReflectDescriptorSet *> descriptor_sets(count);
  SPV_CHECK(reflection_.EnumerateDescriptorSets(&count, descriptor_sets.data()));
  auto stage = getStage();
  std::vector<std::pair<uint32_t, DescriptorSetLayoutBindings>> res;
  std::transform(descriptor_sets.begin(), descriptor_sets.end(), std::back_inserter(res),
                 [&](const SpvReflectDescriptorSet *set) {
                   return std::make_pair(set->set, getDescriptorSetLayoutBindings(set, stage));
                 });
  return res;
}

std::optional<vk::PushConstantRange> ShaderModule::getPushConstantRange() const {
  uint32_t count = 0;
  SPV_CHECK(reflection_.EnumeratePushConstantBlocks(&count, nullptr));
  if (!count)
    return {};
  assert(count == 1);
  SpvReflectBlockVariable *push_constant_block;
  SPV_CHECK(reflection_.EnumeratePushConstantBlocks(&count, &push_constant_block));
  return vk::PushConstantRange{getStage(), push_constant_block->offset, push_constant_block->size};
};

ShaderModule ShaderModuleCache::create(const std::string &name) {
  std::filesystem::path path = shaders_path / name;
  spdlog::info("[gfx] Loading shader module from {}", path.string());
  std::ifstream f(path, std::ios::in | std::ios::binary);
  std::vector<char> code{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  return ShaderModule(device_, {reinterpret_cast<uint32_t *>(code.data()),
                                reinterpret_cast<uint32_t *>(code.data() + code.size())});
}
} // namespace gfx
