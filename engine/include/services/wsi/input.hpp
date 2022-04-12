#ifndef INPUT_HPP
#define INPUT_HPP

struct GLFWwindow;

namespace wsi {
class Input {
public:
  Input(GLFWwindow *window) : window_(window){};
  void processEvents() const;

private:
  GLFWwindow *window_;
};
}

#endif
