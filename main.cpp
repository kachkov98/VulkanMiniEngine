#include "engine.hpp"
#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <cxxopts.hpp>
#include <imgui_impl_glfw.h>

#include <chrono>
#include <thread>

class Example : public vme::Application {
public:
  Example() : vme::Application("Example", {0, 0, 1}) {}

private:
  bool shouldClose() override {
    return vme::Engine::get<wsi::Window>().shouldClose() ||
           vme::Engine::get<wsi::Input>().isKeyPressed(GLFW_KEY_ESCAPE);
  }

  void onInit() override {
    ImGui_ImplGlfw_InitForVulkan(vme::Engine::get<wsi::Window>().getHandle(), true);
    // Create pipeline
    auto &context = vme::Engine::get<gfx::Context>();
    auto &shader_module_cache = context.getShaderModuleCache();
    auto format = context.getSurfaceFormat().format;
    vk::PipelineRasterizationStateCreateInfo raster_state{{},
                                                          false,
                                                          false,
                                                          vk::PolygonMode::eFill,
                                                          vk::CullModeFlagBits::eNone,
                                                          vk::FrontFace::eClockwise,
                                                          false,
                                                          0.f,
                                                          0.f,
                                                          0.f,
                                                          1.f};
    vk::PipelineColorBlendAttachmentState blend_state(
        true, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eA);
    pipeline_ =
        gfx::GraphicsPipelineBuilder(context.getPipelineCache(), context.getPipelineLayoutCache(),
                                     context.getDescriptorSetLayoutCache())
            .shaderStage(shader_module_cache.get("shader.vert.spv"))
            .shaderStage(shader_module_cache.get("shader.frag.spv"))
            .inputAssembly({{}, vk::PrimitiveTopology::eTriangleList})
            .rasterization(raster_state)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(format, blend_state)
            .build();

    imgui_pipeline_ =
        gfx::GraphicsPipelineBuilder(context.getPipelineCache(), context.getPipelineLayoutCache(),
                                     context.getDescriptorSetLayoutCache())
            .shaderStage(shader_module_cache.get("imgui.vert.spv"))
            .shaderStage(shader_module_cache.get("imgui.frag.spv"))
            .inputAssembly({{}, vk::PrimitiveTopology::eTriangleList})
            .rasterization(raster_state)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(format, blend_state)
            .build();
  }

  void onTerminate() override {
    imgui_pipeline_.reset();
    pipeline_.reset();
    ImGui_ImplGlfw_Shutdown();
  }

  void onUpdate(double delta) override {}

  void onRender(double alpha) override {
#if 0
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
#endif
    auto &context = vme::Engine::get<gfx::Context>();
    auto &frame = context.getCurrentFrame();
    auto cmd_buf = frame.getCommandBuffer();
    auto extent = context.getSwapchainExtent();
    try {
      context.acquireNextImage(frame.getImageAvailableSemaphore());
      frame.reset();
      vk::RenderingAttachmentInfo color_attachment{
          context.getCurrentImageView(),
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ResolveModeFlagBits::eNone,
          {},
          vk::ImageLayout::eUndefined,
          vk::AttachmentLoadOp::eClear,
          vk::AttachmentStoreOp::eStore,
          vk::ClearValue(vk::ClearColorValue(std::array{0.f, 0.f, 1.f, 0.f}))};
      vk::ImageMemoryBarrier2 image_barrier1{
          vk::PipelineStageFlagBits2::eTopOfPipe,
          vk::AccessFlagBits2::eNone,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eUndefined,
          vk::ImageLayout::eColorAttachmentOptimal,
          {},
          {},
          context.getCurrentImage(),
          vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
      vk::ImageMemoryBarrier2 image_barrier2{
          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::PipelineStageFlagBits2::eBottomOfPipe,
          vk::AccessFlagBits2::eNone,
          vk::ImageLayout::eColorAttachmentOptimal,
          vk::ImageLayout::ePresentSrcKHR,
          {},
          {},
          context.getCurrentImage(),
          vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
      cmd_buf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
      {
        TracyVkZone(frame.getTracyVkCtx(), cmd_buf, "Render pass");
        cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier1});
        cmd_buf.beginRendering(
            {vk::RenderingFlags{}, vk::Rect2D{vk::Offset2D{0, 0}, extent}, 1, 0, color_attachment});
        pipeline_.bind(cmd_buf);
        cmd_buf.setViewport(
            0, vk::Viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f));
        cmd_buf.setScissor(0, vk::Rect2D(vk::Offset2D(), extent));
        cmd_buf.draw(3, 1, 0, 0);

        cmd_buf.endRendering();
        cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier2});
      }
      TracyVkCollect(frame.getTracyVkCtx(), cmd_buf);
      cmd_buf.end();
      frame.submit();
      context.presentImage(frame.getRenderFinishedSemaphore());
    } catch (vk::OutOfDateKHRError &) {
      auto &context = vme::Engine::get<gfx::Context>();
      auto &window = vme::Engine::get<wsi::Window>();
      context.waitIdle();
      context.recreateSwapchain(window.getFramebufferSize());
    }
  }

private:
  gfx::Pipeline pipeline_;
  gfx::Pipeline imgui_pipeline_;
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("VulkanMiniEngine", "Experimental GPU-driven Vulkan renderer");
  auto result = options.parse(argc, argv);
  Example example;
  try {
    vme::Engine::init();
    example.run(30);
    vme::Engine::terminate();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
