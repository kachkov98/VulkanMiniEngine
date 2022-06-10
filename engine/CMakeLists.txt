add_library(engine STATIC)

target_sources(engine
  PRIVATE
    "src/engine.cpp"
    "src/services/wsi/window.cpp"
    "src/services/wsi/input.cpp"
    "src/services/gfx/context.cpp"
    "src/services/gfx/allocator.cpp"
    "src/services/gfx/descriptors.cpp"
    "src/services/gfx/pipelines.cpp"
    "src/services/gfx/shaders.cpp"
    "src/renderer/render_graph.cpp")

target_include_directories(engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(engine PUBLIC cxx_std_20)

target_compile_definitions(engine
  PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1)

target_link_libraries(engine
  PUBLIC
    cxxopts::cxxopts
    EnTT::EnTT
    glfw
    gli
    glm::glm
    imgui::imgui
    imguizmo::imguizmo
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Taskflow::Taskflow
    Tracy::TracyClient
    unofficial::spirv-reflect::spirv-reflect
    unofficial::vulkan-memory-allocator::vulkan-memory-allocator
    Vulkan::Vulkan)
