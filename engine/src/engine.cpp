#include "engine.hpp"

#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <taskflow/taskflow.hpp>

#include <stdexcept>

namespace vme {

using Executor = entt::locator<tf::Executor>;
using Window = entt::locator<wsi::Window>;
using Input = entt::locator<wsi::Input>;
using Context = entt::locator<gfx::Context>;

void Engine::init() {
  spdlog::info("Engine initialization started");
  // GLFW
  if (!glfwInit())
    throw std::runtime_error("Failed to initialize GLFW");
  spdlog::info("GLFW intialized successfully");
  // Thread pool
  Executor::emplace(/*default number of workers*/);
  spdlog::info("Created thread pool with {} workers", Executor::value().num_workers());
  // Window
  Window::emplace("VulkanMiniEngine");
  spdlog::info("Window created successfully");
  // Input
  Input::emplace(Window::value().getHandle());
  spdlog::info("Input callbacks created successfully");
  // Graphics context
  Context::emplace(Window::value());
  spdlog::info("Created context with device {}",
               Context::value().getPhysicalDevice().getProperties().deviceName);

  spdlog::info("Engine initialized successfully");
}

void Engine::terminate() {
  spdlog::info("Engine termination started");
  Context::value().waitIdle();
  Context::value().savePipelineCache();
  Context::reset();
  Input::reset();
  Window::reset();
  Executor::reset();
  glfwTerminate();
  spdlog::info("Engine terminated successfully");
}

void Engine::run(Application &app, unsigned update_freq) {
  const double delta = 1. / update_freq;
  // Application initialization
  app.onInit();
  double previous = glfwGetTime(), lag = 0.;
  while (!app.shouldClose()) {
    double current = glfwGetTime(), elapsed = current - previous;
    previous = current;
    lag += elapsed;
    // Process input
    get<wsi::Input>().pollEvents();
    // Process fixed-time updates
    while (lag >= delta) {
      app.onUpdate(delta);
      lag -= delta;
    }
    // Process render
    auto &context = get<gfx::Context>();
    auto &frame = context.getCurrentFrame();
    frame.reset();
    context.acquireNextImage(frame.getImageAvailableSemaphore());
    app.onRender(lag / delta);
    frame.submit(get<gfx::Context>().getMainQueue());
    context.presentImage(frame.getRenderFinishedSemaphore());
    get<gfx::Context>().nextFrame();
  }
  // Application termination
  app.onTerminate();
}

} // namespace vme
