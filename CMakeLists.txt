cmake_minimum_required (VERSION 3.21)
project("VulkanMiniEngine")

find_package(Vulkan REQUIRED)

add_subdirectory(engine)
add_subdirectory(examples)
add_subdirectory(shaders)
