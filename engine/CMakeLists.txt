find_package(EnTT CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(unofficial-im3d CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)
find_package(unofficial-vulkan-memory-allocator CONFIG REQUIRED)

find_path(JOLT_INCLUDE_DIR Jolt/Jolt.h)
find_library(JOLT_LIBRARY Jolt)

add_library(engine STATIC)

target_sources(engine
  PRIVATE
    "src/engine.cpp"
    "src/scene/scene.cpp"
    "src/services/wsi/window.cpp"
    "src/services/wsi/input.cpp"
    "src/services/gfx/context.cpp"
    "src/services/gfx/frame.cpp"
    "src/services/gfx/allocator.cpp"
    "src/services/gfx/descriptors.cpp"
    "src/services/gfx/pipelines.cpp"
    "src/services/gfx/resources.cpp"
    "src/services/gfx/shaders.cpp"
    "src/services/gfx/staging_buffer.cpp"
    "src/services/gfx/swapchain.cpp"
    "src/renderer/render_graph.cpp"
    "src/renderer/forward_pass.cpp"
    "src/renderer/imgui_pass.cpp"
    "src/renderer/present_pass.cpp")

target_include_directories(engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${JOLT_INCLUDE_DIR}
)

target_compile_features(engine PUBLIC cxx_std_20)

target_compile_definitions(engine
  PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    IM3D_CONFIG="configs/im3d.h"
    IMGUI_USER_CONFIG="configs/imgui.h")

target_link_libraries(engine
  PUBLIC
    EnTT::EnTT
    glfw
    glm::glm
    imgui::imgui
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Tracy::TracyClient
    unofficial::im3d::im3d
    unofficial::spirv-reflect::spirv-reflect
    unofficial::vulkan-memory-allocator::vulkan-memory-allocator
    Vulkan::Vulkan
    ${JOLT_LIBRARY})
