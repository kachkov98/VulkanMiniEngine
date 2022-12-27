find_package(cxxopts CONFIG REQUIRED)

add_executable(example
  "example.cpp")

add_dependencies(example shaders)

target_link_libraries(example
  PRIVATE
    cxxopts::cxxopts
    engine)
