#ifndef WINDOW_HPP
#define WINDOW_HPP

#include <glm/vec2.hpp>
#include <string>

struct GLFWwindow;

namespace wsi {
class Window final {
public:
  Window() : window_(nullptr){};
  Window(const std::string &title, bool fullscreen = false);
  Window(const Window &rhs) = delete;
  Window(Window &&rhs) noexcept : window_(rhs.window_) { rhs.window_ = nullptr; }
  Window &operator=(const Window &rhs) = delete;
  Window &operator=(Window &&rhs) noexcept {
    window_ = rhs.window_;
    rhs.window_ = nullptr;
    return *this;
  }
  ~Window();

  void setTitle(const std::string &title) const noexcept;
  void setFullscreen(bool fullscreen) const noexcept;
  bool isFullscreen() const noexcept;
  bool isVisible() const noexcept;
  bool isFocused() const noexcept;
  bool shouldClose() const noexcept;
  glm::vec2 getContentScale() const noexcept;
  glm::uvec2 getFramebufferSize() const noexcept;
  GLFWwindow *getHandle() const noexcept { return window_; }

private:
  GLFWwindow *window_;
};

} // namespace wsi

#endif
