#ifndef IMGUI_PASS_HPP
#define IMGUI_PASS_HPP

#include "render_graph.hpp"

#include "services/gfx/allocator.hpp"
#include "services/gfx/pipelines.hpp"

#include <glm/vec2.hpp>

namespace rg {
class ImGuiPass final : public Pass {
public:
  ImGuiPass();

protected:
  void setup(PassBuilder &builder) override;
  void execute(gfx::Frame &frame) override;

private:
  struct PushConstant {
    glm::vec2 scale, translate;
  };

  gfx::Pipeline pipeline_;
  vma::UniqueImage font_texture_;
  vk::UniqueImageView font_texture_view_;
  vk::UniqueSampler font_sampler_;
  gfx::DescriptorSet descriptor_set_;
};
} // namespace rg

#endif
