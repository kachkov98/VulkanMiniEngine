#include "renderer/render_graph.hpp"

#include "services/gfx/context.hpp"

#include <ostream>

namespace rg {

void RenderGraph::compile() {}

void RenderGraph::execute(gfx::Frame &frame) {
  auto memory_pool = frame.getMemoryPool();
  auto command_buffer = frame.getCommandBuffer();
  command_buffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
  command_buffer.end();
}

void RenderGraph::dump(std::ostream &os) const {}

std::ostream &operator<<(std::ostream &os, const RenderGraph &RG) {
  RG.dump(os);
  return os;
}

} // namespace rg
