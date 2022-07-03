#include "renderer/forward_pass.hpp"

#include "engine.hpp"
#include "services/gfx/context.hpp"

#include <glm/gtc/matrix_transform.hpp>

namespace rg {
ForwardPass::ForwardPass(const vme::Scene &scene)
    : Pass("Forward", vk::PipelineStageFlagBits2::eAllGraphics), scene_(&scene) {
  auto &context = vme::Engine::get<gfx::Context>();
  onSwapchainResize(context.getSwapchainExtent());
  // Create pipeline
  {
    auto &shader_module_cache = context.getShaderModuleCache();
    vk::PipelineRasterizationStateCreateInfo raster_state{{},
                                                          false,
                                                          false,
                                                          vk::PolygonMode::eFill,
                                                          vk::CullModeFlagBits::eNone,
                                                          vk::FrontFace::eCounterClockwise,
                                                          false,
                                                          0.f,
                                                          0.f,
                                                          0.f,
                                                          1.f};
    vk::PipelineColorBlendAttachmentState blend_state{
        false,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eZero,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eA};
    pipeline_ =
        gfx::GraphicsPipelineBuilder(context.getPipelineCache(), context.getPipelineLayoutCache(),
                                     context.getDescriptorSetLayoutCache())
            .resourceDescriptorHeap(0, context.getSampledImageDescriptorHeap())
            .resourceDescriptorHeap(1, context.getSamplerDescriptorHeap())
            .shaderStage(shader_module_cache.get("shader.vert.spv"))
            .shaderStage(shader_module_cache.get("shader.frag.spv"))
            .vertexBinding({0, sizeof(glm::vec3), vk::VertexInputRate::eVertex})
            .vertexAttribute({0, 0, vk::Format::eR32G32B32Sfloat, 0})
            .vertexBinding({1, sizeof(glm::vec3), vk::VertexInputRate::eVertex})
            .vertexAttribute({1, 1, vk::Format::eR32G32B32Sfloat, 0})
            .vertexBinding({2, sizeof(glm::vec2), vk::VertexInputRate::eVertex})
            .vertexAttribute({2, 2, vk::Format::eR32G32Sfloat, 0})
            .inputAssembly({{}, vk::PrimitiveTopology::eTriangleList})
            .rasterization(raster_state)
            .depthStencil({{}, true, true, vk::CompareOp::eLess})
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(context.getSurfaceFormat().format, blend_state)
            .depthAttachment(depth_format_)
            .build();
  }
  // Upload transforms and materials and allocate descriptor set
  transforms_ = context.getAllocator().createBufferUnique(
      {{},
       scene_->getTransforms().size() * sizeof(glm::mat4),
       vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst},
      {{}, VMA_MEMORY_USAGE_AUTO});
  context.getStagingBuffer().uploadBuffer<glm::mat4>(
      transforms_->getBuffer(), scene_->getTransforms(),
      vk::BufferCopy2{0, 0, scene_->getTransforms().size() * sizeof(glm::mat4)});
  materials_ = context.getAllocator().createBufferUnique(
      {{},
       scene_->getMaterials().size() * sizeof(vme::Scene::Material),
       vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst},
      {{}, VMA_MEMORY_USAGE_AUTO});
  context.getStagingBuffer().uploadBuffer<vme::Scene::Material>(
      materials_->getBuffer(), scene_->getMaterials(),
      vk::BufferCopy2{0, 0, scene_->getMaterials().size() * sizeof(vme::Scene::Material)});
  const vk::DescriptorBufferInfo transforms_info{transforms_->getBuffer(), 0, VK_WHOLE_SIZE};
  const vk::DescriptorBufferInfo materials_info{materials_->getBuffer(), 0, VK_WHOLE_SIZE};
  descriptor_set_ = gfx::DescriptorSetBuilder(context.getDescriptorSetAllocator(),
                                              context.getDescriptorSetLayoutCache())
                        .bindBuffer(0, vk::DescriptorType::eStorageBuffer, transforms_info,
                                    vk::ShaderStageFlagBits::eVertex)
                        .bindBuffer(1, vk::DescriptorType::eStorageBuffer, materials_info,
                                    vk::ShaderStageFlagBits::eFragment)
                        .build();
}

void ForwardPass::onSwapchainResize(vk::Extent2D extent) {
  auto &context = vme::Engine::get<gfx::Context>();
  depth_image_ =
      context.getAllocator().createImageUnique({{},
                                                vk::ImageType::e2D,
                                                depth_format_,
                                                vk::Extent3D{extent.width, extent.height, 1},
                                                1,
                                                1,
                                                vk::SampleCountFlagBits::e1,
                                                vk::ImageTiling::eOptimal,
                                                vk::ImageUsageFlagBits::eDepthStencilAttachment},
                                               {{}, VMA_MEMORY_USAGE_AUTO});
  depth_image_view_ =
      context.getDevice().createImageViewUnique({{},
                                                 depth_image_->getImage(),
                                                 vk::ImageViewType::e2D,
                                                 depth_format_,
                                                 {},
                                                 {vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1}});
}

void ForwardPass::setup(PassBuilder &builder) {}

void ForwardPass::execute(gfx::Frame &frame) {
  auto &context = vme::Engine::get<gfx::Context>();
  auto extent = context.getSwapchainExtent();
  auto cmd_buf = frame.getCommandBuffer();
  vk::RenderingAttachmentInfo color_attachment{
      context.getCurrentImageView(),
      vk::ImageLayout::eColorAttachmentOptimal,
      vk::ResolveModeFlagBits::eNone,
      {},
      vk::ImageLayout::eUndefined,
      vk::AttachmentLoadOp::eClear,
      vk::AttachmentStoreOp::eStore,
      vk::ClearValue(vk::ClearColorValue(std::array{0.f, 0.25f, 1.f, 0.f}))};
  vk::RenderingAttachmentInfo depth_attachment{*depth_image_view_,
                                               vk::ImageLayout::eDepthAttachmentOptimal,
                                               vk::ResolveModeFlagBits::eNone,
                                               {},
                                               vk::ImageLayout::eUndefined,
                                               vk::AttachmentLoadOp::eClear,
                                               vk::AttachmentStoreOp::eStore,
                                               vk::ClearValue(vk::ClearDepthStencilValue(1.f, 0))};
  cmd_buf.beginRendering(
      {vk::RenderingFlags{}, vk::Rect2D{{}, extent}, 1, 0, color_attachment, &depth_attachment});
  pipeline_.bind(cmd_buf);
  cmd_buf.setViewport(
      0, vk::Viewport{0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f});
  cmd_buf.setScissor(0, vk::Rect2D{{}, extent});
  pipeline_.bindDesriptorSets(cmd_buf, 2, descriptor_set_);
  static float angle = 0.f;
  glm::vec3 camera_pos{2.f * glm::cos(angle), 2.f * glm::sin(angle), -.5f};
  angle += 0.001f;
  glm::mat4 view = glm::lookAt(camera_pos, glm::vec3{}, glm::vec3{0.f, 0.f, 1.f});
  glm::mat4 proj = glm::perspective(glm::half_pi<float>(),
                                    (float)extent.width / (float)extent.height, .1f, 100.f);
  pipeline_.setPushConstant<glm::mat4>(
      cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      offsetof(PushConstant, view_proj), proj * view);
  pipeline_.setPushConstant<glm::vec3>(
      cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
      offsetof(PushConstant, camera_pos), camera_pos);
  for (const auto &mesh : scene_->getMeshes()) {
    pipeline_.setPushConstant<uint32_t>(
        cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
        offsetof(PushConstant, transform_id), mesh.transform_id);
    for (const auto &primitive : mesh.primitives) {
      std::vector<vk::Buffer> vertex_buffers;
      vertex_buffers.reserve(primitive.attributes.size());
      for (const auto &view : primitive.attributes)
        vertex_buffers.push_back(view.buffer);

      std::vector<vk::DeviceSize> vertex_offsets;
      vertex_offsets.reserve(primitive.attributes.size());
      for (const auto &view : primitive.attributes)
        vertex_offsets.push_back(view.offset);

      cmd_buf.bindVertexBuffers(0, vertex_buffers, vertex_offsets);
      cmd_buf.bindIndexBuffer(primitive.indices.buffer, primitive.indices.offset,
                              vk::IndexType::eUint16);
      pipeline_.setPushConstant<uint32_t>(
          cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
          offsetof(PushConstant, material_id), primitive.material_id);
      cmd_buf.drawIndexed(primitive.count, 1, 0, 0, 0);
    }
  }
  cmd_buf.endRendering();
}
} // namespace rg
