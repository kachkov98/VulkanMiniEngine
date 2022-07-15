#include "engine.hpp"
#include "renderer/forward_pass.hpp"
#include "renderer/imgui_pass.hpp"
#include "scene/scene.hpp"
#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <cxxopts.hpp>
#include <imgui_impl_glfw.h>
#include <spdlog/spdlog.h>
#include <tiny_gltf.h>

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
    // Load model
    {
      tinygltf::TinyGLTF loader;
      tinygltf::Model model;
      std::string err, warn;
      if (!loader.LoadBinaryFromFile(&model, &err, &warn, "../../../DamagedHelmet.glb")) {
        if (!warn.empty())
          spdlog::warn("[tinygltf] {}", warn);
        if (!err.empty())
          spdlog::error("[tinygltf] {}", err);
        throw std::runtime_error("Failed to parse glTF");
      }
      scene_ = std::make_unique<vme::Scene>(vme::Engine::get<gfx::Context>(), model);
    }
    // Create pass nodes
    forward_pass_ = std::make_unique<rg::ForwardPass>(*scene_);
    imgui_pass_ = std::make_unique<rg::ImGuiPass>();
    // Flush all pending operations
    vme::Engine::get<gfx::Context>().flush();
  }

  void onTerminate() override {
    imgui_pass_.reset();
    forward_pass_.reset();
    scene_.reset();
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
    auto &swapchain = context.getSwapchain();
    auto &frame = context.getCurrentFrame();
    auto cmd_buf = frame.getCommandBuffer();
    auto extent = swapchain.getExtent();
    if (recreateSwapchainIfNeeded(swapchain.acquireImage(frame.getImageAvailableSemaphore())))
      return;
    frame.reset();
    vk::RenderingAttachmentInfo color_attachment{
        swapchain.getCurrentImageView(),
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
        swapchain.getCurrentImage(),
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    vk::ImageMemoryBarrier2 image_barrier2{
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentRead,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::eColorAttachmentOptimal,
        {},
        {},
        swapchain.getCurrentImage(),
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    vk::ImageMemoryBarrier2 image_barrier3{
        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
        vk::AccessFlagBits2::eColorAttachmentWrite,
        vk::PipelineStageFlagBits2::eBottomOfPipe,
        vk::AccessFlagBits2::eNone,
        vk::ImageLayout::eColorAttachmentOptimal,
        vk::ImageLayout::ePresentSrcKHR,
        {},
        {},
        swapchain.getCurrentImage(),
        vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};
    cmd_buf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier1});
    forward_pass_->doExecute(frame);
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier2});
    imgui_pass_->doExecute(frame);
    cmd_buf.pipelineBarrier2({vk::DependencyFlags{}, {}, {}, image_barrier3});
    TracyVkCollect(frame.getTracyVkCtx(), cmd_buf);
    cmd_buf.end();
    frame.submit();
    if (recreateSwapchainIfNeeded(swapchain.presentImage(frame.getRenderFinishedSemaphore())))
      return;
  }

private:
  std::unique_ptr<vme::Scene> scene_;
  std::unique_ptr<rg::Pass> forward_pass_, imgui_pass_;

  bool recreateSwapchainIfNeeded(vk::Result result) const {
    switch (result) {
    case vk::Result::eSuccess:
      return false;
    case vk::Result::eErrorOutOfDateKHR:
    case vk::Result::eSuboptimalKHR: {
      auto &context = vme::Engine::get<gfx::Context>();
      auto &window = vme::Engine::get<wsi::Window>();
      context.waitIdle();
      context.getSwapchain().recreate(window.getFramebufferSize());
      static_cast<rg::ForwardPass *>(forward_pass_.get())
          ->onSwapchainResize(context.getSwapchain().getExtent());
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
