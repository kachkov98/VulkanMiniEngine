#include "renderer/imgui_pass.hpp"

#include "engine.hpp"
#include "services/gfx/context.hpp"

#include <glm/common.hpp>
#include <imgui.h>

namespace rg {
ImGuiPass::ImGuiPass() : Pass("ImGui", vk::PipelineStageFlagBits2::eAllGraphics) {
  auto &context = vme::Engine::get<gfx::Context>();
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
            .resourceDescriptorHeap(0, context.getSampledImageDescriptorHeap())
            .resourceDescriptorHeap(1, context.getSamplerDescriptorHeap())
            .shaderStage(shader_module_cache.get("imgui.vert.spv"))
            .shaderStage(shader_module_cache.get("imgui.frag.spv"))
            .vertexBinding({0, sizeof(ImDrawVert), vk::VertexInputRate::eVertex})
            .vertexAttribute({0, 0, vk::Format::eR32G32Sfloat, IM_OFFSETOF(ImDrawVert, pos)})
            .vertexAttribute({1, 0, vk::Format::eR32G32Sfloat, IM_OFFSETOF(ImDrawVert, uv)})
            .vertexAttribute({2, 0, vk::Format::eR8G8B8A8Unorm, IM_OFFSETOF(ImDrawVert, col)})
            .inputAssembly({{}, vk::PrimitiveTopology::eTriangleList})
            .rasterization(raster_state)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(context.getSwapchain().getFormat(), blend_state)
            .build();
  }
  // Create font resources
  {
    unsigned char *pixels;
    int width, height, bytes_per_pixel;
    ImFontAtlas *Font = ImGui::GetIO().Fonts;
    Font->AddFontDefault();
    Font->GetTexDataAsRGBA32(&pixels, &width, &height, &bytes_per_pixel);
    const vk::Extent3D image_extent{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    const vk::Format image_format = vk::Format::eR8G8B8A8Unorm;
    const vk::ImageSubresourceLayers subresource_layers{vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    const vk::ImageSubresourceRange subresource_range{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    // Create image
    font_image_ = gfx::Image(context.getAllocator(), {{},
                                                      vk::ImageType::e2D,
                                                      image_format,
                                                      image_extent,
                                                      1,
                                                      1,
                                                      vk::SampleCountFlagBits::e1,
                                                      vk::ImageTiling::eOptimal,
                                                      vk::ImageUsageFlagBits::eTransferDst |
                                                          vk::ImageUsageFlagBits::eSampled});
    font_image_.upload(
        context.getStagingBuffer(), vk::ImageLayout::eUndefined,
        vk::ImageLayout::eShaderReadOnlyOptimal, subresource_range,
        gfx::StagingBuffer::Data<unsigned char>{
            static_cast<uint32_t>(width * height * bytes_per_pixel), pixels},
        vk::BufferImageCopy2{0, 0, 0, subresource_layers, vk::Offset3D{}, image_extent});
    uint32_t texture_index =
        font_image_.allocate(context.getSampledImageDescriptorHeap(),
                             {vk::ImageViewType::e2D, image_format, {}, subresource_range},
                             vk::ImageLayout::eShaderReadOnlyOptimal);
    // Create sampler
    font_sampler_ = gfx::Sampler(
        context.getDevice(),
        {{}, vk::Filter::eLinear, vk::Filter::eLinear, vk::SamplerMipmapMode::eLinear});
    uint32_t sampler_index = font_sampler_.allocate(context.getSamplerDescriptorHeap());
    // Set ImTextureID
    static_assert(sizeof(ImTextureID) == sizeof(uint64_t),
                  "Can not pack texture and sampler index into ImTextureID");
    uint64_t packed_index = ((uint64_t)texture_index << 32) | sampler_index;
    ImGui::GetIO().Fonts->SetTexID(reinterpret_cast<ImTextureID>(packed_index));
  }
}

void ImGuiPass::setup(PassBuilder &builder){};

void ImGuiPass::execute(gfx::Frame &frame) {
  auto &context = vme::Engine::get<gfx::Context>();
  ImDrawData *draw_data = ImGui::GetDrawData();
  glm::vec2 display_pos = draw_data->DisplayPos, display_size = draw_data->DisplaySize,
            framebuffer_scale = draw_data->FramebufferScale;
  glm::vec2 scale = 2.f / display_size;
  glm::vec2 translate = -1.f - display_pos * scale;
  glm::vec2 framebuffer_size = display_size * framebuffer_scale;
  // Allocate vertex and index buffers
  if (!draw_data->TotalVtxCount)
    return;
  auto [vertex_buffer, vertex_data] = frame.getAllocator().createBuffer<ImDrawVert>(
      vk::BufferUsageFlagBits::eVertexBuffer, draw_data->TotalVtxCount);
  auto [index_buffer, index_data] = frame.getAllocator().createBuffer<ImDrawIdx>(
      vk::BufferUsageFlagBits::eIndexBuffer, draw_data->TotalIdxCount);

  auto cmd_buf = frame.getCommandBuffer();
  vk::RenderingAttachmentInfo color_attachment{context.getSwapchain().getCurrentImageView(),
                                               vk::ImageLayout::eColorAttachmentOptimal};
  cmd_buf.beginRendering(
      {vk::RenderingFlags{}, vk::Rect2D{{}, context.getSwapchain().getExtent()}, 1, 0, color_attachment});
  pipeline_.bind(cmd_buf);
  pipeline_.setPushConstant<TransformData>(
      cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment, 0,
      TransformData{scale, translate});

  cmd_buf.bindVertexBuffers(0, vertex_buffer, vk::DeviceSize{0});
  cmd_buf.bindIndexBuffer(index_buffer, vk::DeviceSize{0},
                          sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32);
  cmd_buf.setViewport(0, vk::Viewport{0.f, 0.f, framebuffer_size.x, framebuffer_size.y, 0.f, 1.f});
  size_t vertex_offset = 0, index_offset = 0;
  for (int n = 0; n < draw_data->CmdListsCount; ++n) {
    const ImDrawList *cmd_list = draw_data->CmdLists[n];
    std::copy(cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Data + cmd_list->VtxBuffer.Size,
              vertex_data + vertex_offset);
    std::copy(cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Data + cmd_list->IdxBuffer.Size,
              index_data + index_offset);
    for (int i = 0; i < cmd_list->CmdBuffer.Size; ++i) {
      const auto &cmd = cmd_list->CmdBuffer[i];
      // User callbacks not supported yet
      assert(cmd.UserCallback == nullptr);
      uint64_t packed_index = reinterpret_cast<uint64_t>(cmd.GetTexID());
      uint32_t texture_index = packed_index >> 32;
      uint32_t sampler_index = packed_index & UINT32_MAX;
      pipeline_.setPushConstant<ResourceIndices>(
          cmd_buf, vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment,
          sizeof(TransformData), ResourceIndices{texture_index, sampler_index});
      glm::vec2 clip_min =
          glm::clamp((glm::vec2(cmd.ClipRect.x, cmd.ClipRect.y) - display_pos) * framebuffer_scale,
                     glm::vec2(), framebuffer_size);
      glm::vec2 clip_max =
          glm::clamp((glm::vec2(cmd.ClipRect.z, cmd.ClipRect.w) - display_pos) * framebuffer_scale,
                     glm::vec2(), framebuffer_size);
      glm::ivec2 offset = clip_min;
      glm::uvec2 extent = clip_max - clip_min;
      cmd_buf.setScissor(0, vk::Rect2D{{offset.x, offset.y}, {extent.x, extent.y}});
      cmd_buf.drawIndexed(cmd.ElemCount, 1, index_offset + cmd.IdxOffset,
                          vertex_offset + cmd.VtxOffset, 0);
    }
    vertex_offset += cmd_list->VtxBuffer.Size;
    index_offset += cmd_list->IdxBuffer.Size;
  }
  cmd_buf.endRendering();
};
} // namespace rg
