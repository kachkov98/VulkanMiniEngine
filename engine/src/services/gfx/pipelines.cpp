#include "services/gfx/pipelines.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace gfx {
static const std::filesystem::path cache_path =
    std::filesystem::current_path() / "shader_cache.bin";

PipelineLayoutBuilder &PipelineLayoutBuilder::shaderStage(const ShaderModule &shader_module) {
  const auto &descriptor_set_layouts = shader_module.getDescriptorSetLayouts();
  descriptor_set_layouts_.insert(descriptor_set_layouts_.end(), descriptor_set_layouts.begin(),
                                 descriptor_set_layouts.end());
  if (auto push_constant_range = shader_module.getPushConstantRange())
    push_constant_ranges_.push_back(*push_constant_range);
  return *this;
}

static DescriptorSetLayoutBindings
mergeDescriptorSetLayoutBindings(const DescriptorSetLayoutBindings &bindings) {
  std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> merged_bindings;
  for (const auto &binding : bindings)
    if (auto [it, res] = merged_bindings.insert({binding.binding, binding}); !res)
      it->second.stageFlags |= binding.stageFlags;
  DescriptorSetLayoutBindings res;
  std::transform(merged_bindings.begin(), merged_bindings.end(), std::back_inserter(res),
                 [](const std::pair<uint32_t, vk::DescriptorSetLayoutBinding> &binding) {
                   return binding.second;
                 });
  std::sort(res.begin(), res.end());
  return res;
}

vk::PipelineLayout PipelineLayoutBuilder::build() {
  // Merge descriptor set layouts
  unsigned num_descriptor_sets = 0;
  for (const auto &[id, bindings] : descriptor_set_layouts_)
    num_descriptor_sets = std::max(num_descriptor_sets, id);
  std::vector<DescriptorSetLayoutBindings> descriptor_set_layouts(num_descriptor_sets);
  for (const auto &[id, bindings] : descriptor_set_layouts_)
    std::copy(bindings.begin(), bindings.end(), std::back_inserter(descriptor_set_layouts[id]));

  std::vector<vk::DescriptorSetLayout> merged_descriptor_set_layouts;
  std::transform(descriptor_set_layouts.begin(), descriptor_set_layouts.end(),
                 std::back_inserter(merged_descriptor_set_layouts),
                 [&](const DescriptorSetLayoutBindings &bindings) {
                   auto merged_bindings = mergeDescriptorSetLayoutBindings(bindings);
                   return *descriptor_set_layout_cache_->get({{}, merged_bindings});
                 });

  return *pipeline_layout_cache_->get({{}, merged_descriptor_set_layouts, push_constant_ranges_});
};

PipelineCache::PipelineCache(vk::Device device) : device_(device) {
  spdlog::info("[gfx] Loading shader cache from {}", cache_path.string());
  std::ifstream f(cache_path, std::ios::in | std::ios::binary);
  std::vector<char> cache_data{std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()};
  pipeline_cache_ = device_.createPipelineCacheUnique({{}, cache_data.size(), cache_data.data()});
}

void PipelineCache::save() const {
  spdlog::info("[gfx] Saving shader cache to {}", cache_path.string());
  std::vector<uint8_t> data{device_.getPipelineCacheData(*pipeline_cache_)};
  std::ofstream f(cache_path, std::ios::out | std::ios::binary);
  std::copy(data.begin(), data.end(), std::ostreambuf_iterator<char>(f));
}

vk::UniquePipeline ComputePipelineBuilder::create(vk::PipelineLayout pipeline_layout) {
  return pipeline_cache_->create(
      vk::ComputePipelineCreateInfo{{}, shader_stages_.front(), pipeline_layout});
}

vk::UniquePipeline GraphicsPipelineBuilder::create(vk::PipelineLayout pipeline_layout) {
  vk::PipelineVertexInputStateCreateInfo vertex_input_state{
      {}, vertex_bindings_, vertex_attributes_};
  vk::PipelineViewportStateCreateInfo viewport_state{{}, viewports_, scissors_};
  vk::PipelineColorBlendStateCreateInfo color_blend_state{
      {}, logic_op_enable_, logic_op_, blend_states_, blend_constants_};
  vk::PipelineDynamicStateCreateInfo dynamic_state{{}, dynamic_states_};

  return pipeline_cache_->create(vk::StructureChain{
      vk::GraphicsPipelineCreateInfo{{},
                                     shader_stages_,
                                     &vertex_input_state,
                                     &input_assembly_state_,
                                     &tesselation_state_,
                                     &viewport_state,
                                     &rasterization_state_,
                                     &multisample_state_,
                                     &depth_stencil_state_,
                                     &color_blend_state,
                                     &dynamic_state,
                                     pipeline_layout},
      vk::PipelineRenderingCreateInfo{
          0, color_attachments_, depth_attachment_,
          stencil_attachment_}}.get());
}
} // namespace gfx
