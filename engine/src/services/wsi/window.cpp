#include "services/wsi/window.hpp"
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <tuple>

namespace wsi {
static std::tuple<GLFWmonitor *, glm::ivec2, glm::uvec2> getWindowParams(bool fullscreen) noexcept {
  GLFWmonitor *monitor = glfwGetPrimaryMonitor();
  if (fullscreen) {
    const GLFWvidmode *mode = glfwGetVideoMode(monitor);
    return {monitor, glm::ivec2{}, glm::uvec2(mode->width, mode->height)};
  }
  int width, height;
  glfwGetMonitorWorkarea(monitor, nullptr, nullptr, &width, &height);
  return {nullptr, glm::ivec2{100, 100}, glm::vec2(width, height) * 0.75f};
}

Window::Window(const std::string &title, bool fullscreen) {
  auto [monitor, offset, extent] = getWindowParams(fullscreen);
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  window_ = glfwCreateWindow(extent.x, extent.y, title.c_str(), monitor, nullptr);
  if (!window_)
    throw std::runtime_error("glfwCreateWindow failed");
  glfwSetWindowPos(window_, offset.x, offset.y);
}

Window::~Window() {
  if (window_)
    glfwDestroyWindow(window_);
}

void Window::setTitle(const std::string &title) const noexcept {
  glfwSetWindowTitle(window_, title.c_str());
}

void Window::setFullscreen(bool fullscreen) const noexcept {
  auto [monitor, offset, extent] = getWindowParams(fullscreen);
  glfwSetWindowMonitor(window_, monitor, offset.x, offset.y, extent.x, extent.y, GLFW_DONT_CARE);
}

bool Window::isFullscreen() const noexcept { return glfwGetWindowMonitor(window_); }

bool Window::isVisible() const noexcept { return glfwGetWindowAttrib(window_, GLFW_VISIBLE); }

bool Window::isFocused() const noexcept { return glfwGetWindowAttrib(window_, GLFW_FOCUSED); }

bool Window::shouldClose() const noexcept { return glfwWindowShouldClose(window_); }

glm::vec2 Window::getContentScale() const noexcept {
  float xscale, yscale;
  glfwGetWindowContentScale(window_, &xscale, &yscale);
  return glm::vec2(xscale, yscale);
}

glm::uvec2 Window::getFramebufferSize() const noexcept {
  int width, height;
  glfwGetFramebufferSize(window_, &width, &height);
  return glm::uvec2(width, height);
}
} // namespace wsi
