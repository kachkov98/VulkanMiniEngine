#ifndef PIPELINES_HPP
#define PIPELINES_HPP

#include "common/cache.hpp"
#include "descriptors.hpp"
#include "shaders.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_hash.hpp>

#include <unordered_map>
#include <unordered_set>
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

class PipelineLayoutBuilder {
public:
  PipelineLayoutBuilder(PipelineLayoutCache &pipeline_layout_cache,
                        DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : pipeline_layout_cache_(&pipeline_layout_cache),
        descriptor_set_layout_cache_(&descriptor_set_layout_cache) {}

  PipelineLayoutBuilder &shaderStage(const ShaderModule &shader_module);
  PipelineLayoutBuilder &resourceDescriptorHeap(uint32_t id, vk::DescriptorSetLayout layout) {
    resource_descriptor_heap_layouts_.emplace(id, layout);
    return *this;
  }
  vk::PipelineLayout build();

private:
  PipelineLayoutCache *pipeline_layout_cache_{nullptr};
  DescriptorSetLayoutCache *descriptor_set_layout_cache_{nullptr};

  std::unordered_map<uint32_t, DescriptorSetLayoutBindings> descriptor_set_layouts_;
  std::unordered_map<uint32_t, vk::DescriptorSetLayout> resource_descriptor_heap_layouts_;
  std::vector<vk::PushConstantRange> push_constant_ranges_;
};

class PipelineCache final {
public:
  PipelineCache() = default;
  PipelineCache(vk::Device device);

  void save() const;

  vk::UniquePipeline create(const vk::ComputePipelineCreateInfo &create_info) const;
  vk::UniquePipeline create(const vk::GraphicsPipelineCreateInfo &create_info) const;

private:
  vk::Device device_ = {};
  vk::UniquePipelineCache pipeline_cache_;
};

class Pipeline final {
public:
  Pipeline() = default;
  Pipeline(vk::UniquePipeline &&pipeline, vk::PipelineLayout layout,
           vk::PipelineBindPoint bind_point,
           std::vector<std::pair<const uint32_t, vk::DescriptorSet>> &&resource_descriptor_heaps)
      : pipeline_(std::move(pipeline)), layout_(layout), bind_point_(bind_point),
        resource_descriptor_heaps_(resource_descriptor_heaps) {}

  vk::Pipeline get() const noexcept { return *pipeline_; }
  vk::PipelineLayout getLayout() const noexcept { return layout_; }
  void reset() noexcept {
    pipeline_.reset();
    layout_ = nullptr;
    bind_point_ = {};
  }

  void bind(vk::CommandBuffer cmd_buf) const {
    cmd_buf.bindPipeline(bind_point_, *pipeline_);
    for (const auto &[id, descriptor_set] : resource_descriptor_heaps_)
      cmd_buf.bindDescriptorSets(bind_point_, layout_, id, descriptor_set, {});
  };

  void bindDesriptorSets(vk::CommandBuffer cmd_buf, uint32_t id,
                         const vk::ArrayProxy<const vk::DescriptorSet> &descriptor_sets,
                         const vk::ArrayProxy<const uint32_t> &dynamic_offsets = {}) const {
    cmd_buf.bindDescriptorSets(bind_point_, layout_, id, descriptor_sets, dynamic_offsets);
  };

  template <typename DataType>
  void setPushConstant(vk::CommandBuffer cmd_buf, vk::ShaderStageFlags stages, uint32_t offset,
                       const vk::ArrayProxy<const DataType> &data) const {
    cmd_buf.pushConstants(layout_, stages, offset, data);
  };

private:
  vk::UniquePipeline pipeline_ = {};
  vk::PipelineLayout layout_ = {};
  vk::PipelineBindPoint bind_point_ = {};

  std::vector<std::pair<const uint32_t, vk::DescriptorSet>> resource_descriptor_heaps_;
};

template <typename Derived> class PipelineBuilder : public PipelineLayoutBuilder {
public:
  PipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                  DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : PipelineLayoutBuilder(pipeline_layout_cache, descriptor_set_layout_cache),
        pipeline_cache_(&pipeline_cache) {}

  Derived &shaderStage(const ShaderModule &shader_module,
                       const vk::SpecializationInfo *specialization_info = nullptr) {
    auto stage = shader_module.getStage();
    assert(stage | Derived::shader_stages);
    shader_stages_.emplace(
        stage, vk::PipelineShaderStageCreateInfo{
                   {}, stage, shader_module.get(), shader_module.getName(), specialization_info});
    PipelineLayoutBuilder::shaderStage(shader_module);
    return static_cast<Derived &>(*this);
  }

  Derived &resourceDescriptorHeap(uint32_t id, const ResourceDescriptorHeap &heap) {
    resource_descriptor_heaps_.emplace(id, heap.get());
    PipelineLayoutBuilder::resourceDescriptorHeap(id, heap.getLayout());
    return static_cast<Derived &>(*this);
  }

  Pipeline build() {
    std::vector resource_descriptor_heaps(resource_descriptor_heaps_.begin(),
                                          resource_descriptor_heaps_.end());
    auto layout = PipelineLayoutBuilder::build();
    return Pipeline(static_cast<Derived *>(this)->create(layout), layout, Derived::bind_point,
                    std::move(resource_descriptor_heaps));
  }

protected:
  PipelineCache *pipeline_cache_{nullptr};

  std::unordered_map<vk::ShaderStageFlagBits, vk::PipelineShaderStageCreateInfo> shader_stages_;
  std::unordered_map<uint32_t, vk::DescriptorSet> resource_descriptor_heaps_;
};

class ComputePipelineBuilder final : public PipelineBuilder<ComputePipelineBuilder> {
public:
  static constexpr vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eCompute;
  static constexpr vk::ShaderStageFlags shader_stages = vk::ShaderStageFlagBits::eCompute;

  ComputePipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                         DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : PipelineBuilder(pipeline_cache, pipeline_layout_cache, descriptor_set_layout_cache) {}

  vk::UniquePipeline create(vk::PipelineLayout pipeline_layout);
};

class GraphicsPipelineBuilder final : public PipelineBuilder<GraphicsPipelineBuilder> {
public:
  static constexpr vk::PipelineBindPoint bind_point = vk::PipelineBindPoint::eGraphics;
  static constexpr vk::ShaderStageFlags shader_stages = vk::ShaderStageFlagBits::eAllGraphics;

  GraphicsPipelineBuilder(PipelineCache &pipeline_cache, PipelineLayoutCache &pipeline_layout_cache,
                          DescriptorSetLayoutCache &descriptor_set_layout_cache)
      : PipelineBuilder(pipeline_cache, pipeline_layout_cache, descriptor_set_layout_cache) {}

  vk::UniquePipeline create(vk::PipelineLayout pipeline_layout);

  GraphicsPipelineBuilder &vertexBinding(const vk::VertexInputBindingDescription &binding) {
    vertex_bindings_.push_back(binding);
    return *this;
  }
  GraphicsPipelineBuilder &vertexAttribute(const vk::VertexInputAttributeDescription &attribute) {
    vertex_attributes_.push_back(attribute);
    return *this;
  }

  GraphicsPipelineBuilder &
  inputAssembly(const vk::PipelineInputAssemblyStateCreateInfo &input_assembly_state) {
    input_assembly_state_ = input_assembly_state;
    return *this;
  }

  GraphicsPipelineBuilder &
  tesselation(const vk::PipelineTessellationStateCreateInfo &tesselation_state) {
    tesselation_state_ = tesselation_state;
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

  GraphicsPipelineBuilder &
  rasterization(const vk::PipelineRasterizationStateCreateInfo &rasterization_state) {
    rasterization_state_ = rasterization_state;
    return *this;
  }

  GraphicsPipelineBuilder &
  multisample(const vk::PipelineMultisampleStateCreateInfo &multisample_state) {
    multisample_state_ = multisample_state;
    return *this;
  }

  GraphicsPipelineBuilder &
  depthStencil(const vk::PipelineDepthStencilStateCreateInfo &depth_stencil_state) {
    depth_stencil_state_ = depth_stencil_state;
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
    dynamic_states_.insert(dynamic_state);
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
  std::vector<vk::VertexInputBindingDescription> vertex_bindings_;
  std::vector<vk::VertexInputAttributeDescription> vertex_attributes_;

  vk::PipelineInputAssemblyStateCreateInfo input_assembly_state_ = {};
  vk::PipelineTessellationStateCreateInfo tesselation_state_ = {};

  std::vector<vk::Viewport> viewports_;
  std::vector<vk::Rect2D> scissors_;

  vk::PipelineRasterizationStateCreateInfo rasterization_state_ = {};
  vk::PipelineMultisampleStateCreateInfo multisample_state_ = {};
  vk::PipelineDepthStencilStateCreateInfo depth_stencil_state_ = {};

  bool logic_op_enable_ = {};
  vk::LogicOp logic_op_ = vk::LogicOp::eClear;
  std::vector<vk::PipelineColorBlendAttachmentState> blend_states_;
  std::array<float, 4> blend_constants_ = {};

  std::unordered_set<vk::DynamicState> dynamic_states_;

  std::vector<vk::Format> color_attachments_;
  vk::Format depth_attachment_ = vk::Format::eUndefined;
  vk::Format stencil_attachment_ = vk::Format::eUndefined;
};
} // namespace gfx

#endif
