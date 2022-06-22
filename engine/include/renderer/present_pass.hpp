#ifndef PRESENT_PASS_HPP
#define PRESENT_PASS_HPP

#include "render_graph.hpp"

namespace rg {
class PresentPass final : public Pass {
public:
  PresentPass() : Pass("Present", vk::PipelineStageFlagBits2::eCopy, true) {}

protected:
  void setup(PassBuilder &builder) override;
  void execute(gfx::Frame &frame) override;
};
} // namespace rg

#endif
