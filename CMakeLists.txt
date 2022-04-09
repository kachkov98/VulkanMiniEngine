cmake_minimum_required (VERSION 3.21)
project("VulkanMiniEngine")

find_package(cxxopts CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(gli CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(VulkanMiniEngine
  "main.cpp"
  "renderer/window.cpp"
  "renderer/context.cpp"
  "renderer/allocator.cpp"
  "renderer/descriptors.cpp"
  "renderer/framegraph.cpp")

target_compile_features(VulkanMiniEngine PRIVATE cxx_std_20)

target_compile_definitions(VulkanMiniEngine
  PRIVATE
  VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
  VMA_STATIC_VULKAN_FUNCTIONS=0
  VMA_DYNAMIC_VULKAN_FUNCTIONS=1)

target_link_libraries(VulkanMiniEngine
  PRIVATE
  cxxopts::cxxopts
  glfw
  gli
  glm::glm
  imgui::imgui
  imguizmo::imguizmo
  nlohmann_json::nlohmann_json
  spdlog::spdlog
  unofficial::spirv-reflect::spirv-reflect
  unofficial::vulkan-memory-allocator::vulkan-memory-allocator
  Vulkan::Vulkan)
