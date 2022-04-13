#include "engine.hpp"

#include "services/gfx/context.hpp"
#include "services/wsi/input.hpp"
#include "services/wsi/window.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>
#include <taskflow/taskflow.hpp>

#include <stdexcept>

namespace vme {

using Executor = entt::service_locator<tf::Executor>;
using Window = entt::service_locator<wsi::Window>;
using Input = entt::service_locator<wsi::Input>;
using Context = entt::service_locator<gfx::Context>;

void Engine::init() {
  spdlog::info("Engine initialization started");
  // GLFW
  if (!glfwInit())
    throw std::runtime_error("Failed to initialize GLFW");
  spdlog::info("GLFW intialized successfully");
  // Thread pool
  Executor::set(/*default number of workers*/);
  spdlog::info("Created thread pool with {} workers", Executor::ref().num_workers());
  // Window
  Window::set("VulkanMiniEngine");
  spdlog::info("Window created successfully");
  // Input
  Input::set(Window::ref().getHandle());
  spdlog::info("Input callbacks created successfully");
  // Graphics context
  Context::set(Window::ref());
  spdlog::info("Created context with device {}",
               Context::ref().getPhysicalDevice().getProperties().deviceName);

  spdlog::info("Engine initialized successfully");
}

void Engine::terminate() {
  spdlog::info("Engine termination started");
  Context::ref().savePipelineCache();
  Context::reset();
  Input::reset();
  Window::reset();
  Executor::reset();
  glfwTerminate();
  spdlog::info("Engine terminated successfully");
}
} // namespace vme
