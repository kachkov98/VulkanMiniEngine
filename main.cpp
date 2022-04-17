#include "engine.hpp"
#include "services/wsi/window.hpp"
#include "services/wsi/input.hpp"
#include <cxxopts.hpp>

int main(int argc, char *argv[]) {
  cxxopts::Options options("VulkanMiniEngine", "Experimental GPU-driven Vulkan renderer");
  auto result = options.parse(argc, argv);
  try {
    vme::Engine::init();
    while (!vme::Engine::get<wsi::Window>().shouldClose())
      vme::Engine::get<wsi::Input>().pollEvents();
    vme::Engine::terminate();
  } catch (const std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return 0;
}
