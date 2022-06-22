cmake_minimum_required (VERSION 3.21)
project("VulkanMiniEngine")

find_package(cxxopts CONFIG REQUIRED)
find_package(EnTT CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)
find_package(Ktx CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Taskflow CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)
find_package(Vulkan REQUIRED)

add_subdirectory(engine)
add_subdirectory(shaders)

add_executable(example
  "main.cpp")

add_dependencies(example shaders)

target_link_libraries(example
  PRIVATE
    engine)
