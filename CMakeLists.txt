# Copyright (c) 2024-2025 ManifoldFR

cmake_minimum_required(VERSION 3.26)

set(PROJECT_NAME candlewick)
set(PROJECT_CUSTOM_HEADER_EXTENSION "h")
set(PROJECT_DESCRIPTION "A tiny cross-platform renderer based on SDL3")
set(PROJECT_CUSTOM_HEADER_DIR)
set(PROJECT_URL "https://github.com/Simple-Robotics/candlewick")
set(PROJECT_GENERATED_HEADERS_SKIP_WARNING True)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CXX_DISABLE_WERROR True)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PROJECT_USE_CMAKE_EXPORT True)

set(DOXYGEN_USE_MATHJAX "YES")
set(DOXYGEN_FILE_PATTERNS "*.h *.hh *.hpp *.hxx")
set(
  DOXYGEN_INPUT
  "${CMAKE_CURRENT_LIST_DIR}/src ${CMAKE_CURRENT_LIST_DIR}/README.md"
)

# The following was copied over from https://github.com/Simple-Robotics/aligator:
# Use BoostConfig module distributed by boost library instead of using FindBoost module distributed
# by CMake
if(POLICY CMP0167)
  cmake_policy(SET CMP0167 NEW)
  # find_package(SDFormat14) reset this policy (with cmake_minimum_required) and redefine the find_dependency macro.
  # Then, futur call of this macro don't use the right version of this policy.
  # To avoid that, we define the CMP0167 default value.
  set(CMAKE_POLICY_DEFAULT_CMP0167 NEW)
endif()
include(cmake/base.cmake)
COMPUTE_PROJECT_ARGS(PROJECT_ARGS LANGUAGES CXX)
project(${PROJECT_NAME} ${PROJECT_ARGS})

include(cmake/ide.cmake)
include(cmake/apple.cmake)

# Remove a CXX compiler flag (added by jrl-cmakemodules)
macro(remove_cxx_flag name)
  string(REPLACE ${name} "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
endmacro()

remove_cxx_flag(-pedantic)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/$<CONFIG>/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/$<CONFIG>/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/$<CONFIG>/bin)
set(CMAKE_VERBOSE_MAKEFILE False)

set(AWESOME_CSS_DIR ${PROJECT_SOURCE_DIR}/doc/doxygen-awesome-css)

option(BUILD_EXAMPLES "Build examples." OFF)
option(BUILD_PINOCCHIO_VISUALIZER "Build the Pinocchio visualizer." ON)

option(BUILD_PYTHON_BINDINGS "Build Python bindings." OFF)
cmake_dependent_option(
  GENERATE_PYTHON_STUBS
  "Generate the Python stubs associated to the Python library"
  OFF
  BUILD_PYTHON_BINDINGS
  OFF
)

set(
  CMAKE_MODULE_PATH
  "${PROJECT_SOURCE_DIR}/cmake/find-external/Simde"
  "${PROJECT_SOURCE_DIR}/cmake/find-external/FFmpeg"
  ${CMAKE_MODULE_PATH}
)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(
    CACHE CMAKE_BUILD_TYPE
    PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
  )
endif()

ADD_PROJECT_DEPENDENCY(SDL3 3.2.4 REQUIRED)
ADD_PROJECT_DEPENDENCY(assimp REQUIRED)
ADD_PROJECT_DEPENDENCY(Eigen3 REQUIRED)
ADD_PROJECT_DEPENDENCY(coal REQUIRED PKG_CONFIG_REQUIRES "coal >= 3.0.0")
if(BUILD_PINOCCHIO_VISUALIZER)
  ADD_PROJECT_DEPENDENCY(
    pinocchio
    REQUIRED
    PKG_CONFIG_REQUIRES "pinocchio >= 3.7.0"
  )
endif()
set(CANDLEWICK_ASSETS_DIR ${PROJECT_SOURCE_DIR}/assets)
set(CANDLEWICK_SHADER_SRC_DIR ${PROJECT_SOURCE_DIR}/shaders)
set(
  CANDLEWICK_SHADER_INSTALL_DIR
  ${CMAKE_INSTALL_FULL_DATADIR}/${PROJECT_NAME}/shaders
)
message(STATUS "Shader install dir: ${CANDLEWICK_SHADER_INSTALL_DIR}")

add_subdirectory(external)
add_subdirectory(src)

if(BUILD_EXAMPLES)
  add_subdirectory(examples)
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

if(BUILD_PYTHON_BINDINGS)
  add_subdirectory(bindings/python)
  # WIP nanobind bindings
  # add_subdirectory(bindings/nanobind)
endif()

# install shaders
install(
  DIRECTORY "${CANDLEWICK_SHADER_SRC_DIR}/compiled"
  DESTINATION ${CANDLEWICK_SHADER_INSTALL_DIR}
  FILES_MATCHING
  PATTERN "*.json"
  PATTERN "*.spv"
  PATTERN "*.msl"
)
