#include "services/gfx/pipelines.hpp"

#include <spdlog/spdlog.h>

#include <filesystem>
#include <fstream>

namespace gfx {
static const std::filesystem::path cache_path =
    std::filesystem::current_path() / "shader_cache.bin";

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

std::vector<vk::DescriptorSetLayout>
mergeDescriptorSetLayouts(const DescriptorSetLayouts &descriptor_set_layouts,
                          DescriptorSetLayoutCache &descriptor_set_layout_cache) {
  unsigned num_descriptor_sets = 0;
  for (const auto &[id, bindings] : descriptor_set_layouts)
    num_descriptor_sets = std::max(num_descriptor_sets, id);
  std::vector<std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding>> merged_layouts(
      num_descriptor_sets);
  for (const auto &[id, bindings] : descriptor_set_layouts) {
  }

  std::vector<vk::DescriptorSetLayout> res;
  std::transform(merged_layouts.begin(), merged_layouts.end(), std::back_inserter(res),
                 [&](const std::unordered_map<uint32_t, vk::DescriptorSetLayoutBinding> &bindings) {
                   auto bindings_vec = getValuesVector(bindings);
                   return *descriptor_set_layout_cache.get({{}, bindings_vec});
                 });
  return res;
}

std::unordered_map<std::string, vk::PushConstantRange>
mergePushConstantRanges(const PushConstantRanges &push_constant_ranges) {
  std::unordered_map<std::string, vk::PushConstantRange> res;
  for (const auto &[name, push_constant_range] : push_constant_ranges) {
    auto it = res.find(name);
    if (it == res.end()) {
      res.insert({name, push_constant_range});
      continue;
    }
    auto &merged_push_constant_range = it->second;
    merged_push_constant_range.stageFlags |= push_constant_range.stageFlags;
    assert(merged_push_constant_range.offset = push_constant_range.offset);
    assert(merged_push_constant_range.size = push_constant_range.size);
  }
  return res;
}

vk::UniquePipeline ComputePipelineBuilder::create(vk::PipelineLayout pipeline_layout) {
  return pipeline_cache_->create(
      vk::ComputePipelineCreateInfo{{}, shader_stages_.front(), pipeline_layout});
}

vk::UniquePipeline GraphicsPipelineBuilder::create(vk::PipelineLayout pipeline_layout) {
  vk::PipelineVertexInputStateCreateInfo vertex_input_state{};
  vk::PipelineInputAssemblyStateCreateInfo input_assembly_state{{}, topology_, restart_enable_};
  vk::PipelineTessellationStateCreateInfo tesselation_state{{}, patch_control_points_};
  vk::PipelineViewportStateCreateInfo viewport_state{{}, viewports_, scissors_};
  vk::PipelineRasterizationStateCreateInfo rasterization_state{{},
                                                               depth_clamp_enable_,
                                                               rasterizer_discard_enable_,
                                                               polygon_mode_,
                                                               cull_mode_,
                                                               front_face_,
                                                               depth_bias_enable_,
                                                               depth_bias_constant_factor_,
                                                               depth_bias_clamp_,
                                                               depth_bias_slope_factor_,
                                                               line_width_};
  vk::PipelineMultisampleStateCreateInfo multisample_state{
      {},           rasterization_samples_,    sample_shading_enable_, min_sample_shading_,
      sample_mask_, alpha_to_coverage_enable_, alpha_to_one_enable_};
  vk::PipelineDepthStencilStateCreateInfo depth_stencil_state{{},
                                                              depth_test_enable_,
                                                              depth_write_enable_,
                                                              depth_compare_op_,
                                                              depth_bounds_test_enable_,
                                                              stencil_test_enable_,
                                                              front_,
                                                              back_,
                                                              min_depth_bounds_,
                                                              max_depth_bounds_};
  vk::PipelineColorBlendStateCreateInfo color_blend_state{
      {}, logic_op_enable_, logic_op_, blend_states_, blend_constants_};
  vk::PipelineDynamicStateCreateInfo dynamic_state{{}, dynamic_states_};

  return pipeline_cache_->create(vk::StructureChain{
      vk::GraphicsPipelineCreateInfo{{},
                                     shader_stages_,
                                     &vertex_input_state,
                                     &input_assembly_state,
                                     &tesselation_state,
                                     &viewport_state,
                                     &rasterization_state,
                                     &multisample_state,
                                     &depth_stencil_state,
                                     &color_blend_state,
                                     &dynamic_state,
                                     pipeline_layout},
      vk::PipelineRenderingCreateInfo{
          0, color_attachments_, depth_attachment_,
          stencil_attachment_}}.get());
}
} // namespace gfx
