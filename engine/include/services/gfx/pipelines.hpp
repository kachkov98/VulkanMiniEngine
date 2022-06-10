#ifndef PIPELINES_HPP
#define PIPELINES_HPP

#include "common/cache.hpp"
#include "descriptors.hpp"
#include "shaders.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <vector>

namespace gfx {

class PipelineLayoutCache final
    : public vme::Cache<PipelineLayoutCache, vk::PipelineLayoutCreateInfo,
                        vk::UniquePipelineLayout> {
public:
  PipelineLayoutCache(vk::Device device = {}) : device_(device) {}

  vk::UniquePipelineLayout create(const vk::PipelineLayoutCreateInfo &create_info) {
    return device_.createPipelineLayoutUnique(create_info);
  }

private:
  vk::Device device_;
};

class PipelineCache final {
public:
  PipelineCache() = default;
  PipelineCache(vk::Device device);

  void save() const;

  vk::UniquePipeline create(const vk::ComputePipelineCreateInfo &create_info) const {
    return device_.createComputePipelineUnique(*pipeline_cache_, create_info);
  }
  vk::UniquePipeline create(const vk::GraphicsPipelineCreateInfo &create_info) const {
    return device_.createGraphicsPipelineUnique(*pipeline_cache_, create_info);
  }

private:
  vk::Device device_ = {};
  vk::UniquePipelineCache pipeline_cache_;
};

class Pipeline final {
public:
  Pipeline() = default;
  Pipeline(vk::UniquePipeline &&pipeline, vk::PipelineLayout layout,
           vk::PipelineBindPoint bind_point,
           std::vector<vk::DescriptorSetLayout> &&descriptor_set_layouts,
           std::unordered_map<std::string, vk::PushConstantRange> &&push_constant_ranges)
      : pipeline_(std::move(pipeline)), layout_(layout), bind_point_(bind_point),
        descriptor_set_layouts_(std::move(descriptor_set_layouts)),
        push_constant_ranges_(std::move(push_constant_ranges)) {}

  vk::Pipeline get() const noexcept { return *pipeline_; }
  vk::PipelineLayout getLayout() const noexcept { return layout_; }
  void reset() noexcept {
    pipeline_.reset();
    layout_ = nullptr;
    bind_point_ = {};
    descriptor_set_layouts_.clear();
    push_constant_ranges_.clear();
  }

  void bind(vk::CommandBuffer cmd_buf) const { cmd_buf.bindPipeline(bind_point_, *pipeline_); };

  void bindDesriptorSet(vk::CommandBuffer cmd_buf, uint32_t id, DescriptorSet descriptor_set,
                        const vk::ArrayProxy<const uint32_t> &dynamic_offsets = {}) const {
    assert(descriptor_set_layouts_.at(id) == descriptor_set.getLayout() &&
           "Incompatible descriptor set layout");
    cmd_buf.bindDescriptorSets(bind_point_, layout_, id, descriptor_set.get(), dynamic_offsets);
  };

  template <typename DataType>
  void setPushConstant(vk::CommandBuffer cmd_buf, std::string name,
                       const vk::ArrayProxy<const DataType> &data) const {
    auto &push_constant_range = push_constant_ranges_.at(name);
    assert(push_constant_range.size == data.size() * sizeof(DataType) &&
           "Invalid push constant size");
    cmd_buf.pushConstants(pipeline_, push_constant_range.stageFlags, push_constant_range.offset,
                          data);
  };

private:
  vk::UniquePipeline pipeline_ = {};
  vk::PipelineLayout layout_ = {};
  vk::PipelineBindPoint bind_point_ = {};

  std::vector<vk::DescriptorSetLayout> descriptor_set_layouts_;
  std::unordered_map<std::string, vk::PushConstantRange> push_constant_ranges_;
};

template <typename Key, typename Value>
std::vector<Value> getValuesVector(const std::unordered_map<Key, Value> &map) {
  std::vector<Value> res;
  std::transform(map.begin(), map.end(), std::back_inserter(res),
                 [](const std::pair<Key, Value> &p) { return p.second; });
  return res;
}

std::vector<vk::DescriptorSetLayout>
mergeDescriptorSetLayouts(const DescriptorSetLayouts &descriptor_set_layouts,
                          DescriptorSetLayoutCache &descriptor_set_layout_cache);
std::unordered_map<std::string, vk::PushConstantRange>
mergePushConstantRanges(const PushConstantRanges &push_constant_ranges);

template <typename Derived> class PipelineBuilder {
public:
  PipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                  DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : pipeline_cache_(&pipeline_cache), pipeline_layout_cache_(&pipeline_layout_cache),
        descriptor_set_layout_cache_(&descriptor_set_layout_cache) {}

  Derived &shaderStage(const ShaderModule &shader_module,
                       const vk::SpecializationInfo *specialization_info = nullptr) {
    shader_stages_.emplace_back(vk::PipelineShaderStageCreateFlags{}, shader_module.getStage(),
                                shader_module.get(), shader_module.getName(), specialization_info);
    const auto &descriptor_set_layouts = shader_module.getDescriptorSetLayouts();
    descriptor_set_layouts_.insert(descriptor_set_layouts_.end(), descriptor_set_layouts.begin(),
                                   descriptor_set_layouts.end());
    const auto &push_constant_ranges = shader_module.getPushConstantRanges();
    push_constant_ranges_.insert(push_constant_ranges_.end(), push_constant_ranges.begin(),
                                 push_constant_ranges.end());
    return static_cast<Derived &>(*this);
  }

  Pipeline build() {
    auto descriptor_set_layouts =
        mergeDescriptorSetLayouts(descriptor_set_layouts_, *descriptor_set_layout_cache_);
    auto push_constant_ranges = mergePushConstantRanges(push_constant_ranges_);
    auto push_constant_ranges_vec = getValuesVector(push_constant_ranges);
    auto &layout =
        *pipeline_layout_cache_->get({{}, descriptor_set_layouts, push_constant_ranges_vec});
    return Pipeline(static_cast<Derived *>(this)->create(layout), layout, Derived::bind_point,
                    std::move(descriptor_set_layouts), std::move(push_constant_ranges));
  };

protected:
  PipelineCache *pipeline_cache_{nullptr};
  PipelineLayoutCache *pipeline_layout_cache_{nullptr};
  DescriptorSetLayoutCache *descriptor_set_layout_cache_{nullptr};

  std::vector<vk::PipelineShaderStageCreateInfo> shader_stages_;
  DescriptorSetLayouts descriptor_set_layouts_;
  PushConstantRanges push_constant_ranges_;
};

class ComputePipelineBuilder final : public PipelineBuilder<ComputePipelineBuilder> {
public:
  static constexpr vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eCompute;

  ComputePipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                         DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : PipelineBuilder(pipeline_cache, pipeline_layout_cache, descriptor_set_layout_cache) {}

  vk::UniquePipeline create(vk::PipelineLayout pipeline_layout);
};

class GraphicsPipelineBuilder final : public PipelineBuilder<GraphicsPipelineBuilder> {
public:
  static constexpr vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics;

  GraphicsPipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                          DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : PipelineBuilder(pipeline_cache, pipeline_layout_cache, descriptor_set_layout_cache) {}

  vk::UniquePipeline create(vk::PipelineLayout pipeline_layout);

  GraphicsPipelineBuilder &
  inputAssembly(vk::PrimitiveTopology topology = vk::PrimitiveTopology::ePointList,
                bool restart_enable = {}) {
    topology_ = topology;
    restart_enable_ = restart_enable;
    return *this;
  }

  GraphicsPipelineBuilder &tesselation(uint32_t patch_control_points = {}) {
    patch_control_points_ = patch_control_points;
    return *this;
  }

  GraphicsPipelineBuilder &viewport(const vk::Viewport &viewport) {
    viewports_.push_back(viewport);
    return *this;
  }
  GraphicsPipelineBuilder &scissor(const vk::Rect2D &scissor) {
    scissors_.push_back(scissor);
    return *this;
  }

  GraphicsPipelineBuilder &rasterization(
      bool depth_clamp_enable = {}, bool rasterizer_discard_enable = {},
      vk::PolygonMode polygon_mode = vk::PolygonMode::eFill, vk::CullModeFlags cull_mode = {},
      vk::FrontFace front_face = vk::FrontFace::eCounterClockwise, bool depth_bias_enable = {},
      float depth_bias_constant_factor = {}, float depth_bias_clamp = {},
      float depth_bias_slope_factor = {}, float line_width = {}) {
    depth_clamp_enable_ = depth_clamp_enable;
    rasterizer_discard_enable_ = rasterizer_discard_enable;
    polygon_mode_ = polygon_mode;
    cull_mode_ = cull_mode;
    front_face_ = front_face;
    depth_bias_enable_ = depth_bias_enable;
    depth_bias_constant_factor_ = depth_bias_constant_factor;
    depth_bias_clamp_ = depth_bias_clamp;
    depth_bias_slope_factor_ = depth_bias_slope_factor;
    line_width_ = line_width;
    return *this;
  }

  GraphicsPipelineBuilder &
  multisample(vk::SampleCountFlagBits rasterization_samples = vk::SampleCountFlagBits::e1,
              bool sample_shading_enable = {}, float min_sample_shading = {},
              const vk::SampleMask *sample_mask = {}, bool alpha_to_coverage_enable = {},
              bool alpha_to_one_enable = {}) {
    rasterization_samples_ = rasterization_samples;
    sample_shading_enable_ = sample_shading_enable;
    min_sample_shading_ = min_sample_shading;
    sample_mask_ = sample_mask;
    alpha_to_coverage_enable_ = alpha_to_coverage_enable;
    alpha_to_one_enable_ = alpha_to_one_enable;
    return *this;
  }

  GraphicsPipelineBuilder &depthStencil(bool depth_test_enable = {}, bool depth_write_enable = {},
                                        vk::CompareOp depth_compare_op = vk::CompareOp::eNever,
                                        bool depth_bounds_test_enable = {},
                                        bool stencil_test_enable = {},
                                        vk::StencilOpState front = {}, vk::StencilOpState back = {},
                                        float min_depth_bounds = {}, float max_depth_bounds = {}) {
    depth_test_enable_ = depth_test_enable;
    depth_write_enable_ = depth_write_enable;
    depth_compare_op_ = depth_compare_op;
    depth_bounds_test_enable_ = depth_bounds_test_enable;
    stencil_test_enable_ = stencil_test_enable;
    front_ = front;
    back_ = back;
    min_depth_bounds_ = min_depth_bounds;
    max_depth_bounds_ = max_depth_bounds;
    return *this;
  }

  GraphicsPipelineBuilder &colorBlend(bool logic_op_enable = {},
                                      vk::LogicOp logic_op = vk::LogicOp::eClear,
                                      const std::array<float, 4> &blend_constants = {}) {
    logic_op_enable_ = logic_op_enable;
    logic_op_ = logic_op;
    blend_constants_ = blend_constants;
    return *this;
  }

  GraphicsPipelineBuilder &dynamicState(vk::DynamicState dynamic_state) {
    dynamic_states_.push_back(dynamic_state);
    return *this;
  };

  GraphicsPipelineBuilder &
  colorAttachment(vk::Format format,
                  const vk::PipelineColorBlendAttachmentState &blend_state = {}) {
    color_attachments_.push_back(format);
    blend_states_.push_back(blend_state);
    return *this;
  }
  GraphicsPipelineBuilder &depthAttachment(vk::Format format = vk::Format::eUndefined) {
    depth_attachment_ = format;
    return *this;
  }
  GraphicsPipelineBuilder &stencilAttachment(vk::Format format = vk::Format::eUndefined) {
    stencil_attachment_ = format;
    return *this;
  }

private:
  vk::PrimitiveTopology topology_ = vk::PrimitiveTopology::ePointList;
  bool restart_enable_ = {};

  uint32_t patch_control_points_ = {};

  std::vector<vk::Viewport> viewports_;
  std::vector<vk::Rect2D> scissors_;

  bool depth_clamp_enable_ = {};
  bool rasterizer_discard_enable_ = {};
  vk::PolygonMode polygon_mode_ = vk::PolygonMode::eFill;
  vk::CullModeFlags cull_mode_ = {};
  vk::FrontFace front_face_ = vk::FrontFace::eCounterClockwise;
  bool depth_bias_enable_ = {};
  float depth_bias_constant_factor_ = {};
  float depth_bias_clamp_ = {};
  float depth_bias_slope_factor_ = {};
  float line_width_ = {};

  vk::SampleCountFlagBits rasterization_samples_ = vk::SampleCountFlagBits::e1;
  bool sample_shading_enable_ = {};
  float min_sample_shading_ = {};
  const vk::SampleMask *sample_mask_ = {};
  bool alpha_to_coverage_enable_ = {};
  bool alpha_to_one_enable_ = {};

  bool depth_test_enable_ = {};
  bool depth_write_enable_ = {};
  vk::CompareOp depth_compare_op_ = vk::CompareOp::eNever;
  bool depth_bounds_test_enable_ = {};
  bool stencil_test_enable_ = {};
  vk::StencilOpState front_ = {};
  vk::StencilOpState back_ = {};
  float min_depth_bounds_ = {};
  float max_depth_bounds_ = {};

  bool logic_op_enable_ = {};
  vk::LogicOp logic_op_ = vk::LogicOp::eClear;
  std::vector<vk::PipelineColorBlendAttachmentState> blend_states_;
  std::array<float, 4> blend_constants_ = {};

  std::vector<vk::DynamicState> dynamic_states_;

  std::vector<vk::Format> color_attachments_;
  vk::Format depth_attachment_ = vk::Format::eUndefined;
  vk::Format stencil_attachment_ = vk::Format::eUndefined;
};
} // namespace gfx

#endif
