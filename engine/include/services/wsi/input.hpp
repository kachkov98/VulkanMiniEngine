#ifndef INPUT_HPP
#define INPUT_HPP

#include <entt/signal/dispatcher.hpp>
#include <glm/vec2.hpp>

#include "window.hpp"

namespace wsi {

struct KeyEvent {};
struct MouseButtonEvent {};
struct ScrollEvent {};

class Input {
public:
  Input(const Window &window) : window_(window.getHandle()){};
  void pollEvents() const noexcept;

  bool isKeyPressed(int key) const noexcept;
  bool isMouseButtonPressed(int button) const noexcept;
  glm::vec2 getCursorPos() const noexcept;

  template <typename Listener> void addListener(Listener &listener) {}

  template <typename Listener> void removeListener(Listener &listener) {}

private:
  GLFWwindow *window_;
  entt::dispatcher dispatcher_;
};
}

#endif
