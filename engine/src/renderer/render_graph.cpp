#include "renderer/render_graph.hpp"

#include "services/gfx/context.hpp"

#include <ostream>

namespace rg {

void Pass::doSetup(PassBuilder &builder) { setup(builder); }

void Pass::doExecute(gfx::Frame &frame) {
  TracyVkZoneTransient(frame.getTracyVkCtx(), __tracy_gpu_zone, frame.getCommandBuffer(),
                       getName().c_str(), true);
  ZoneTransientN(___tracy_scoped_zone, getName().c_str(), true);
  execute(frame);
}

void RenderGraph::compile() {}

void RenderGraph::execute(gfx::Frame &frame) {
  auto cmd_buf = frame.getCommandBuffer();
  cmd_buf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  TracyVkCollect(frame.getTracyVkCtx(), cmd_buf);
  cmd_buf.end();
}

void RenderGraph::dump(std::ostream &os) const {}

std::ostream &operator<<(std::ostream &os, const RenderGraph &RG) {
  RG.dump(os);
  return os;
}

} // namespace rg
