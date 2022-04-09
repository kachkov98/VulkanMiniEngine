#include "renderer/context.hpp"
#include "renderer/window.hpp"
#include <GLFW/glfw3.h>
#include <cxxopts.hpp>
#include <memory>

int main(int argc, char *argv[]) {
  cxxopts::Options options("VulkanMiniEngine", "Experimental GPU-driven Vulkan renderer");
  auto result = options.parse(argc, argv);
  glfwInit();
  try {
    auto window = std::make_unique<wsi::Window>("VulkanMiniEngine");
    auto context = std::make_unique<gfx::Context>(window.get());
    while (!window->shouldClose())
      glfwPollEvents();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  glfwTerminate();
  return 0;
}
