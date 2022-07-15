#include "renderer/present_pass.hpp"

#include "engine.hpp"
#include "services/gfx/context.hpp"

namespace rg {
void PresentPass::setup(PassBuilder &builder) {}
void PresentPass::execute(gfx::Frame &frame) {
  auto &swapchain = vme::Engine::get<gfx::Context>().getSwapchain();
  swapchain.acquireImage(frame.getImageAvailableSemaphore());
  auto cmd_buf = frame.getCommandBuffer();
  cmd_buf.copyImage2(vk::CopyImageInfo2{});
  swapchain.presentImage(frame.getRenderFinishedSemaphore());
}
}
