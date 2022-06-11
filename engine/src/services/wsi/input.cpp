#include "services/wsi/input.hpp"
#include <GLFW/glfw3.h>

namespace wsi {
void Input::pollEvents() const noexcept { glfwPollEvents(); }

bool Input::isKeyPressed(int key) const noexcept { return glfwGetKey(window_, key) == GLFW_PRESS; }

bool Input::isMouseButtonPressed(int button) const noexcept {
  return glfwGetMouseButton(window_, button) == GLFW_PRESS;
}

glm::vec2 Input::getCursorPos() const noexcept {
  double xpos, ypos;
  glfwGetCursorPos(window_, &xpos, &ypos);
  return glm::vec2(xpos, ypos);
}
} // namespace wsi
