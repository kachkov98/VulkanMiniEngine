#include "renderer/render_graph.hpp"

#include "services/gfx/context.hpp"

#include <ostream>

namespace rg {

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
