#include "engine.hpp"
#include "renderer/imgui_pass.hpp"
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
    pass_ = std::make_unique<rg::ImGuiPass>();
    // Flush all pending operations
    auto &context = vme::Engine::get<gfx::Context>();
    context.getBufferDescriptorHeap().flush();
    context.getImageDescriptorHeap().flush();
    context.getTextureDescriptorHeap().flush();
    context.getSamplerDescriptorHeap().flush();
    context.getStagingBuffer().flush();
  }

  void onTerminate() override {
    pass_.reset();
    ImGui_ImplGlfw_Shutdown();
  }

  void onUpdate(double delta) override {}

  void onRender(double alpha) override {
    // Toogle fullscreen
    static bool prev_state = false;
    bool cur_state = vme::Engine::get<wsi::Input>().isKeyPressed(GLFW_KEY_F11);
    if (!prev_state && cur_state) {
      auto &window = vme::Engine::get<wsi::Window>();
      window.setFullscreen(!window.isFullscreen());
    }
    prev_state = cur_state;
    // Collect ImGui data
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow();
    ImGui::Render();
    // Render
    auto &context = vme::Engine::get<gfx::Context>();
    auto &frame = context.getCurrentFrame();
    auto cmd_buf = frame.getCommandBuffer();
    auto extent = context.getSwapchainExtent();
    if (recreateSwapchainIfNeeded(context.acquireNextImage(frame.getImageAvailableSemaphore())))
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
    pass_->doExecute(frame);
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier2});
    TracyVkCollect(frame.getTracyVkCtx(), cmd_buf);
    cmd_buf.end();
    frame.submit();
    if (recreateSwapchainIfNeeded(context.presentImage(frame.getRenderFinishedSemaphore())))
      return;
  }

private:
  std::unique_ptr<rg::Pass> pass_;

  bool recreateSwapchainIfNeeded(vk::Result result) const {
    switch (result) {
    case vk::Result::eSuccess:
      return false;
    case vk::Result::eErrorOutOfDateKHR:
    case vk::Result::eSuboptimalKHR: {
      auto &context = vme::Engine::get<gfx::Context>();
      auto &window = vme::Engine::get<wsi::Window>();
      context.waitIdle();
      context.recreateSwapchain(window.getFramebufferSize());
      return true;
    }
    default:
      vk::throwResultException(result, "recreateSwapchainIfNeeded");
    }
  }
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
