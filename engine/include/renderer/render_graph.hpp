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
  Node(const std::string &name) : name_(name) {}
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

class Resource;
class PassBuilder;

class Pass : public Node {
public:
  Pass(const std::string &name, vk::PipelineStageFlags2 stage, bool has_side_effects = false)
      : Node(name), stage_(stage), has_side_effects_(has_side_effects) {}
  virtual ~Pass() = default;

  vk::PipelineStageFlags2 getStage() const noexcept { return stage_; }
  bool hasSideEffects() const noexcept { return has_side_effects_; }

  void doSetup(PassBuilder &builder);
  void doExecute(gfx::Frame &frame);
  void dump(std::ostream &os) const;

protected:
  virtual void setup(PassBuilder &builder) = 0;
  virtual void execute(gfx::Frame &frame) = 0;

  vk::PipelineStageFlags2 stage_;
  bool has_side_effects_{false};
  std::vector<std::pair<const Resource *, vk::AccessFlags2>> creates_;
  std::vector<std::pair<const Resource *, vk::AccessFlags2>> reads_;
  std::vector<std::pair<const Resource *, vk::AccessFlags2>> writes_;
};

std::ostream &operator<<(std::ostream &os, const Pass &pass);

class Resource : public Node {
public:
  unsigned getVersion() const noexcept { return version_; }
  bool isTransient() const noexcept { return creator_ != nullptr; }
  bool isRetained() const noexcept { return creator_ == nullptr; }

protected:
  virtual void create() = 0;
  virtual void destroy() = 0;
  virtual vk::MemoryRequirements getMemoryRequirements() = 0;

  unsigned version_{0};
  const Pass *creator_{nullptr};
  std::vector<const Pass *> readers_;
  std::vector<const Pass *> writers_;
};

class Buffer : public Resource {};

class Image : public Resource {};

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

  void reset() {
    passes_.clear();
    resources_.clear();
  }

private:
  std::vector<std::unique_ptr<Pass>> passes_;
  std::vector<std::unique_ptr<Resource>> resources_;
};

std::ostream &operator<<(std::ostream &os, const RenderGraph &RG);

class PassBuilder final {
public:
  PassBuilder(RenderGraph &render_graph, Pass &pass)
      : render_graph_(&render_graph), pass_(&pass) {}

private:
  RenderGraph *render_graph_;
  Pass *pass_;
};

} // namespace rg

#endif
