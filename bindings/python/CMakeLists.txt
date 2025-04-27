include(${PROJECT_SOURCE_DIR}/cmake/python.cmake)
if(GENERATE_PYTHON_STUBS)
  include(${PROJECT_SOURCE_DIR}/cmake/stubs.cmake)
endif(GENERATE_PYTHON_STUBS)

find_package(eigenpy 3.2.0 REQUIRED)

set(pycandlewick_SOURCES src/expose-renderer.cpp src/module.cpp)

set(PYLIB_INSTALL_DIR ${PYTHON_SITELIB}/${PROJECT_NAME})

Python3_add_library(pycandlewick MODULE WITH_SOABI ${pycandlewick_SOURCES})
target_link_libraries(pycandlewick PRIVATE candlewick eigenpy::eigenpy)
target_include_directories(
  pycandlewick
  PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
if(BUILD_PINOCCHIO_VISUALIZER)
  target_sources(pycandlewick PRIVATE src/expose-visualizer.cpp)
  # Add definition for the core library
  target_compile_definitions(
    pycandlewick
    PRIVATE CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
  )
endif()

set_target_properties(
  pycandlewick
  PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${PROJECT_NAME}
)
if(UNIX)
  GET_RELATIVE_RPATH(${PYLIB_INSTALL_DIR} PYLIB_INSTALL_RPATH)
  set_target_properties(
    pycandlewick
    PROPERTIES INSTALL_RPATH "${PYLIB_INSTALL_RPATH}"
  )
endif()

install(
  TARGETS pycandlewick
  EXPORT ${TARGETS_EXPORT_NAME}
  LIBRARY DESTINATION ${PYLIB_INSTALL_DIR}
)

if(GENERATE_PYTHON_STUBS)
  LOAD_STUBGEN()
  GENERATE_STUBS(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${PROJECT_NAME}
    ${PYTHON_SITELIB}
    pycandlewick
  )
endif(GENERATE_PYTHON_STUBS)

set(PYTHON_FILES __init__.py)

foreach(pyfile ${PYTHON_FILES})
  PYTHON_INSTALL_ON_SITE(${PROJECT_NAME} __init__.py)
endforeach()
