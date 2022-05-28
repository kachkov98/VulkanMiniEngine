#include "engine.hpp"
#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"
#include <cxxopts.hpp>

class Example : public vme::Application {
public:
  Example() : vme::Application("Example", {0, 0, 1}) {}
  bool shouldClose() override { return vme::Engine::get<wsi::Window>().shouldClose(); }
  void onInit() override {}
  void onTerminate() override {}
  void onUpdate(double delta) override {}
  void onRender(double alpha) override {
    auto &context = vme::Engine::get<gfx::Context>();
    auto &frame = context.getCurrentFrame();
    auto cmd_buf = frame.getCommandBuffer();
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
    cmd_buf.beginRendering({vk::RenderingFlags{},
                            vk::Rect2D{vk::Offset2D{0, 0}, context.getSwapchainExtent()}, 1, 0,
                            color_attachment});
    cmd_buf.endRendering();
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier2});
    cmd_buf.end();
  }
};

int main(int argc, char *argv[]) {
  cxxopts::Options options("VulkanMiniEngine", "Experimental GPU-driven Vulkan renderer");
  auto result = options.parse(argc, argv);
  Example example;
  try {
    vme::Engine::init();
    vme::Engine::run(example, 30);
    vme::Engine::terminate();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
