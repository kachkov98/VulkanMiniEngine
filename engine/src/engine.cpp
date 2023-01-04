#include "engine.hpp"

#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <tracy/Tracy.hpp>
#include <imgui.h>
#include <spdlog/spdlog.h>

#include <stdexcept>

namespace vme {

using Window = entt::locator<wsi::Window>;
using Input = entt::locator<wsi::Input>;
using Context = entt::locator<gfx::Context>;

Application::Application(const std::string &name, const Version &version)
    : name_(name), version_(version) {
  TracyAppInfo(name.c_str(), name.size());
}

void Application::run(unsigned update_freq) {
  const double delta = 1. / update_freq;
  // Application initialization
  {
    ZoneScopedN("Init");
    onInit();
  }
  double previous = glfwGetTime(), lag = 0.;
  while (!shouldClose()) {
    double current = glfwGetTime(), elapsed = current - previous;
    previous = current;
    lag += elapsed;
    // Process input
    Engine::get<wsi::Input>().pollEvents();
    // Process fixed-time updates
    while (lag >= delta) {
      ZoneScopedN("Update");
      onUpdate(delta);
      lag -= delta;
    }
    // Process render
    {
      ZoneScopedN("Render");
      onRender(lag / delta);
    }
    Engine::get<gfx::Context>().nextFrame();
    FrameMark;
  }
  // Application termination
  {
    ZoneScopedN("Terminate");
    Engine::get<gfx::Context>().waitIdle();
    onTerminate();
  }
}

void Engine::init() {
  spdlog::info("Engine initialization started");
  // GLFW
  if (!glfwInit())
    throw std::runtime_error("Failed to initialize GLFW");
  spdlog::info("GLFW intialized successfully");
  // ImGui
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  spdlog::info("ImGui initialized successfully");
  // Window
  Window::emplace("VulkanMiniEngine");
  spdlog::info("Window created successfully");
  // Input
  Input::emplace(Window::value());
  spdlog::info("Input callbacks created successfully");
  // Graphics context
  Context::emplace(Window::value());
  spdlog::info("Vulkan context created successfully");

  spdlog::info("Engine initialized successfully");
}

void Engine::terminate() {
  spdlog::info("Engine termination started");
  Context::value().getPipelineCache().save();
  Context::reset();
  Input::reset();
  Window::reset();
  ImGui::DestroyContext();
  glfwTerminate();
  spdlog::info("Engine terminated successfully");
}

} // namespace vme
