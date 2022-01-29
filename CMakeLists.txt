cmake_minimum_required (VERSION 3.8)
project ("VulkanMiniEngine")

find_package(cxxopts REQUIRED)

add_executable (VulkanMiniEngine "VulkanMiniEngine.cpp" "VulkanMiniEngine.h")

target_link_libraries(VulkanMiniEngine cxxopts::cxxopts)
