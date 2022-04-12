# VulkanMiniEngine
![CI status](https://github.com/kachkov98/VulkanMiniEngine/workflows/CI/badge.svg)  
  
VulkanMiniEngine is an experimental GPU-driven renderer. It is being developed for educational purposes and currently WIP.
## Expected features  
- Windows and Linux support
- Vulkan 1.3 renderer (with use of VK_KHR_dynamic_rendering)
- Barriers handling with render graph
- Fully GPU-driven rendering
- ECS architecture (based on EnTT)
- Dependency management via vcpkg

## Build
### Prerequisites
- GPU with Vulkan 1.3 support is required, please check that your drivers are updated to the latest version (all main IHVs already have support of Vulkan 1.3)
- Vulkan SDK 1.3 (1.3.204 was tested) should be installed (check that VULKAN_SDK environment var is correct)
- On Linux, check that GLFW dependencies are installed (https://www.glfw.org/docs/3.3/compile.html#compile_deps)
- Build tools: cmake 3.21+, ninja

### Steps
```text
# configure
cmake --preset ninja-multi-vcpkg
# Debug build
cmake --build --preset ninja-multi-vcpkg-debug
# Release build
cmake --build --preset ninja-multi-vcpkg-release
```
