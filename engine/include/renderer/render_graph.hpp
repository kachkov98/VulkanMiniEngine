#ifndef RENDER_GRAPH_HPP
#define RENDER_GRAPH_HPP

#include <vulkan/vulkan.hpp>

#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace gfx {
class Frame;
}

namespace rg {
class Node {
public:
  Node(const Node &) = delete;
  Node(Node &&) = default;
  Node &operator=(const Node &) = delete;
  Node &operator=(Node &&) = default;
  virtual ~Node() = default;

  const std::string &getName() const noexcept { return name_; }

protected:
  std::string name_;
  unsigned id_, ref_count_{0};
};

enum class PassType { Graphics, Compute, Present };

class ResourceNode;

class PassNode : public Node {
public:
  PassType getPassType() const noexcept { return pass_type_; }
  bool hasSideEffects() const noexcept { return has_side_effects_; }

protected:
  virtual void setup() = 0;
  virtual void execute() = 0;

  PassType pass_type_;
  bool has_side_effects_{false};
  std::vector<const ResourceNode *> creates_;
  std::vector<const ResourceNode *> reads_;
  std::vector<const ResourceNode *> writes_;
};

class ResourceNode : public Node {
public:
  unsigned getVersion() const noexcept { return version_; }
  bool isTransient() const noexcept { return creator_ != nullptr; }
  bool isRetained() const noexcept { return creator_ == nullptr; }

protected:
  virtual void create() = 0;
  virtual void destroy() = 0;

  unsigned version_{0};
  const PassNode *creator_{nullptr};
  std::vector<const PassNode *> readers_;
  std::vector<const PassNode *> writers_;
};

template <typename Data> class Pass final : public PassNode {};

template <typename Descriptor> class Resource final : public ResourceNode {};

class PassBuilder;

class RenderGraph final {
public:
  RenderGraph() = default;
  RenderGraph(const RenderGraph &) = delete;
  RenderGraph(RenderGraph &&) noexcept = default;
  RenderGraph &operator=(const RenderGraph &) = delete;
  RenderGraph &operator=(RenderGraph &&) noexcept = default;

  template <typename Data> void addPass(const std::string &name, vk::PipelineStageFlags);

  template <typename Descriptor> void addResource(const std::string &name);

  void compile();
  void execute(gfx::Frame &frame);

  void dump(std::ostream &os) const;

private:
  std::vector<std::unique_ptr<PassNode>> passes_;
  std::vector<std::unique_ptr<ResourceNode>> resources_;
};

std::ostream &operator<<(std::ostream &os, const RenderGraph &RG);

class PassBuilder final {
public:
  PassBuilder(RenderGraph &render_graph, PassNode &pass_node)
      : render_graph_(&render_graph), pass_node_(&pass_node) {}

private:
  RenderGraph *render_graph_;
  PassNode *pass_node_;
};

} // namespace rg

#endif
