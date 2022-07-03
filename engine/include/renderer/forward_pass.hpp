#ifndef FORWARD_PASS_HPP
#define FORWARD_PASS_HPP

#include "render_graph.hpp"

#include "scene/scene.hpp"
#include "services/gfx/allocator.hpp"
#include "services/gfx/pipelines.hpp"

namespace rg {
class ForwardPass final : public Pass {
public:
  ForwardPass(const vme::Scene &scene);

  void onSwapchainResize(vk::Extent2D extent);

protected:
  void setup(PassBuilder &builder) override;
  void execute(gfx::Frame &frame) override;

private:
  struct PushConstant {
    glm::mat4 view_proj;
    glm::vec3 camera_pos;
    uint32_t transform_id;
    uint32_t material_id;
  };

  const vme::Scene *scene_;

  const vk::Format depth_format_ = vk::Format::eD32Sfloat;
  vma::UniqueImage depth_image_;
  vk::UniqueImageView depth_image_view_;

  gfx::Pipeline pipeline_;

  vma::UniqueBuffer materials_;
  vma::UniqueBuffer transforms_;
  vk::DescriptorSet descriptor_set_;
};
} // namespace rg

#endif
