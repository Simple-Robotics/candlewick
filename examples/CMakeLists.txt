find_package(CLI11 CONFIG REQUIRED)

add_library(ExamplesCommon STATIC lib/Common.cpp lib/PerlinNoise.cpp)
target_link_libraries(ExamplesCommon PUBLIC candlewick_core imgui_headers)
target_include_directories(
  ExamplesCommon
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/lib
)

if(BUILD_PINOCCHIO_VISUALIZER)
  target_sources(ExamplesCommon PRIVATE lib/GenHeightfield.cpp)
  target_link_libraries(ExamplesCommon PUBLIC candlewick_multibody)

  set(ROBOT_DESCRIPTIONS_CPP_INSTALL OFF CACHE BOOL "")
  set(ROBOT_DESCRIPTIONS_CPP_TESTING OFF CACHE BOOL "")
  add_subdirectory(robot_descriptions_cpp EXCLUDE_FROM_ALL)
endif()

function(add_candlewick_example filename)
  cmake_path(GET filename STEM name)
  message(STATUS "Add C++ example ${name}")
  add_executable(${name} ${filename})
  # always link to ExamplesCommon
  target_link_libraries(${name} PRIVATE ExamplesCommon)
  foreach(arg ${ARGN})
    target_link_libraries(${name} PRIVATE ${arg})
  endforeach()
endfunction()

add_candlewick_example(Triangle.cpp)
add_candlewick_example(ColoredCube.cpp)
add_candlewick_example(MeshNormalsRgb.cpp)
add_candlewick_example(LitMesh.cpp)
if(BUILD_PINOCCHIO_VISUALIZER)
  add_candlewick_example(
    Ur5WithSystems.cpp
    CLI11::CLI11
    robot_descriptions_cpp
  )
  add_candlewick_example(Visualizer.cpp CLI11::CLI11 robot_descriptions_cpp)
endif()
