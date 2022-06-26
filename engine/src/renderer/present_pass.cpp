#include "renderer/present_pass.hpp"

#include "engine.hpp"
#include "services/gfx/context.hpp"

namespace rg {
void PresentPass::setup(PassBuilder &builder) {}
void PresentPass::execute(gfx::Frame &frame) {
  auto &context = vme::Engine::get<gfx::Context>();
  context.acquireNextImage(frame.getImageAvailableSemaphore());
  auto cmd_buf = frame.getCommandBuffer();
  cmd_buf.copyImage2(vk::CopyImageInfo2{});
  context.presentImage(frame.getRenderFinishedSemaphore());
}
}
