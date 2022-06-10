#include "engine.hpp"
#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <cxxopts.hpp>

#include <chrono>
#include <thread>

class Example : public vme::Application {
public:
  Example() : vme::Application("Example", {0, 0, 1}) {}

private:
  bool shouldClose() override { return vme::Engine::get<wsi::Window>().shouldClose(); }
  void onInit() override {
    auto &context = vme::Engine::get<gfx::Context>();
    auto &shader_module_cache = context.getShaderModuleCache();
    auto format = context.getSurfaceFormat().format;
    vk::PipelineColorBlendAttachmentState blend_state(
        false, vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::BlendFactor::eOne, vk::BlendFactor::eZero, vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eB |
            vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eA);
    pipeline_ =
        gfx::GraphicsPipelineBuilder(context.getPipelineCache(), context.getPipelineLayoutCache(),
                                     context.getDescriptorSetLayoutCache())
            .shaderStage(shader_module_cache.get("shader.vert.spv"))
            .shaderStage(shader_module_cache.get("shader.frag.spv"))
            .inputAssembly(vk::PrimitiveTopology::eTriangleList)
            .rasterization(false, false, vk::PolygonMode::eFill, vk::CullModeFlagBits::eNone,
                           vk::FrontFace::eClockwise, false, 0.f, 0.f, 0.f, 1.f)
            .dynamicState(vk::DynamicState::eViewport)
            .dynamicState(vk::DynamicState::eScissor)
            .colorAttachment(format, blend_state)
            .build();
  }
  void onTerminate() override { pipeline_.reset(); }
  void onUpdate(double delta) override {}
  void onRender(double alpha) override {
    auto &context = vme::Engine::get<gfx::Context>();
    auto extent = context.getSwapchainExtent();
    auto &frame = context.getCurrentFrame();
    auto cmd_buf = frame.getCommandBuffer();
    if (!context.acquireNextImage(frame.getImageAvailableSemaphore()))
      return;
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
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier1});
    {
      TracyVkZone(frame.getTracyVkCtx(), cmd_buf, "Render pass");
      cmd_buf.beginRendering({vk::RenderingFlags{},
                              vk::Rect2D{vk::Offset2D{0, 0}, context.getSwapchainExtent()}, 1, 0,
                              color_attachment});
      pipeline_.bind(cmd_buf);
      cmd_buf.setViewport(
          0, vk::Viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f));
      cmd_buf.setScissor(0, vk::Rect2D(vk::Offset2D(), extent));
      cmd_buf.draw(3, 1, 0, 0);

      cmd_buf.endRendering();
    }
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier2});
    TracyVkCollect(frame.getTracyVkCtx(), cmd_buf);
    cmd_buf.end();
    frame.submit();
    if (!context.presentImage(frame.getRenderFinishedSemaphore()))
      return;
  }

private:
  gfx::Pipeline pipeline_;
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
