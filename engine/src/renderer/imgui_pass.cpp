#include "renderer/imgui_pass.hpp"

#include "engine.hpp"
#include "services/gfx/context.hpp"

#include <imgui.h>

namespace rg {
ImGuiPass::ImGuiPass() : Pass("ImGui", vk::PipelineStageFlagBits2::eAllGraphics) {
  auto &context = vme::Engine::get<gfx::Context>();
  extent_ = context.getSwapchainExtent();
  format_ = context.getSurfaceFormat().format;
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
        true,
        vk::BlendFactor::eSrcAlpha,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::BlendFactor::eOne,
        vk::BlendFactor::eOneMinusSrcAlpha,
        vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eA};
    pipeline_ =
        gfx::GraphicsPipelineBuilder(context.getPipelineCache(), context.getPipelineLayoutCache(),
                                     context.getDescriptorSetLayoutCache())
            .shaderStage(shader_module_cache.get("imgui.vert.spv"))
            .shaderStage(shader_module_cache.get("imgui.frag.spv"))
            .inputAssembly({{}, vk::PrimitiveTopology::eTriangleList})
            .rasterization(raster_state)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(format_, blend_state)
            .build();
  }
  // Create buffers

  // Create font texture
  {
    unsigned char *pixels;
    int width, height, bytes_per_pixel;
    ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);
    const vk::Extent3D texture_extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                                      1};
    const vk::Format texture_format = vk::Format::eR8G8B8A8Unorm;
    const vk::ImageSubresourceLayers subresource_layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    const vk::ImageSubresourceRange subresource_range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    font_texture_ = context.getAllocator().createImageUnique(
        {{},
         vk::ImageType::e2D,
         texture_format,
         texture_extent,
         1,
         1,
         vk::SampleCountFlagBits::e1,
         vk::ImageTiling::eOptimal,
         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled},
        {{}, VMA_MEMORY_USAGE_AUTO});
    context.getStagingBuffer().uploadImage(
        font_texture_->getImage(), vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal, subresource_range,
        gfx::StagingBuffer::Data<unsigned char>{
            static_cast<uint32_t>(width * height * bytes_per_pixel), pixels},
        vk::BufferImageCopy2{0, 0, 0, subresource_layers, vk::Offset3D{}, texture_extent});
    // Create font texture view
    font_texture_view_ = context.getDevice().createImageViewUnique({{},
                                                                    font_texture_->getImage(),
                                                                    vk::ImageViewType::e2D,
                                                                    texture_format,
                                                                    {},
                                                                    subresource_range});
  }
  // Create font sampler
  font_sampler_ = context.getDevice().createSamplerUnique(
      {{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear});
  // Create descriptor set
  {
    vk::DescriptorImageInfo image_info{*font_sampler_, *font_texture_view_,
                                       vk::ImageLayout::eShaderReadOnlyOptimal};
    descriptor_set_ = gfx::DescriptorSetBuilder(context.getDescriptorSetAllocator(),
                                                context.getDescriptorSetLayoutCache())
                          .bind(0, vk::DescriptorType::eCombinedImageSampler, image_info,
                                *font_sampler_, vk::ShaderStageFlagBits::eFragment)
                          .build();
  }
}

void ImGuiPass::setup(PassBuilder &builder){};

void ImGuiPass::execute(gfx::Frame &frame) {
  ImDrawData *draw_data = ImGui::GetDrawData();
  glm::vec2 scale = 2.f / glm::vec2{draw_data->DisplaySize.x, draw_data->DisplaySize.y};
  glm::vec2 translate = 1.f - glm::vec2{draw_data->DisplayPos.x, draw_data->DisplayPos.y} * scale;

  auto cmd_buf = frame.getCommandBuffer();
  cmd_buf.beginRendering({});
  pipeline_.bind(cmd_buf);
  pipeline_.bindDesriptorSet(cmd_buf, 0, descriptor_set_.get());
  pipeline_.setPushConstant<PushConstant>(cmd_buf, vk::ShaderStageFlagBits::eVertex, 0,
                                          PushConstant{scale, translate});

  cmd_buf.bindVertexBuffers(0, vertex_buffer_->getBuffer(), vk::DeviceSize{0});
  cmd_buf.bindIndexBuffer(index_buffer_->getBuffer(), vk::DeviceSize{0},
                          sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
  cmd_buf.setViewport(0, vk::Viewport{0.f, 0.f, static_cast<float>(extent_.width),
                                      static_cast<float>(extent_.height), 0.f, 1.f});
  size_t vertex_offset = 0, index_offset = 0;
  for (int n = 0; n < draw_data->CmdListsCount; ++n) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    for (int i = 0; i < cmd_list->CmdBuffer.Size; ++i) {
      const auto &cmd = cmd_list->CmdBuffer[i];
      // User callbacks not supported yet
      assert(cmd.UserCallback == nullptr);
      cmd_buf.setScissor(0, vk::Rect2D{vk::Offset2D{}, vk::Extent2D{}});
      cmd_buf.drawIndexed(cmd.ElemCount, 1, index_offset + cmd.IdxOffset,
                          vertex_offset + cmd.VtxOffset, 0);
    }
    vertex_offset += cmd_list->VtxBuffer.Size;
    index_offset += cmd_list->IdxBuffer.Size;
  }
  cmd_buf.endRendering();
};
} // namespace rg
