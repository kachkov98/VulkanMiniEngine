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
    "src/renderer/render_graph.cpp"
    "src/renderer/forward_pass.cpp"
    "src/renderer/imgui_pass.cpp"
    "src/renderer/present_pass.cpp")

target_include_directories(engine
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(engine PUBLIC cxx_std_20)

target_compile_definitions(engine
  PUBLIC
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VMA_STATIC_VULKAN_FUNCTIONS=0
    VMA_DYNAMIC_VULKAN_FUNCTIONS=1
    IMGUI_USER_CONFIG="imgui_config.h")

target_link_libraries(engine
  PUBLIC
    cxxopts::cxxopts
    EnTT::EnTT
    glfw
    glm::glm
    imgui::imgui
    imguizmo::imguizmo
    KTX::ktx
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    Taskflow::Taskflow
    Tracy::TracyClient
    unofficial::spirv-reflect::spirv-reflect
    unofficial::vulkan-memory-allocator::vulkan-memory-allocator
    Vulkan::Vulkan)
