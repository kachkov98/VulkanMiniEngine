#include "services/wsi/input.hpp"
#include <GLFW/glfw3.h>

namespace wsi {
void Input::processEvents() const { glfwPollEvents(); }
}
