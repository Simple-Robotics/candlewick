if(CMAKE_VERSION VERSION_LESS 3.18)
  set(DEV_MODULE Development)
else()
  set(DEV_MODULE Development.Module)
endif()

find_package(Python 3.10 COMPONENTS Interpreter ${DEV_MODULE} REQUIRED)

execute_process(
  COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir
  OUTPUT_STRIP_TRAILING_WHITESPACE
  OUTPUT_VARIABLE nanobind_ROOT
)
find_package(nanobind CONFIG REQUIRED)

function(_add_nanobind_bindings)
  set(_python_SOURCES module.cpp)
  nanobind_add_module(pycandlewick_nb NB_STATIC NB_SUPPRESS_WARNINGS ${_python_SOURCES})
  target_link_libraries(pycandlewick_nb PRIVATE candlewick)
  set_target_properties(
    pycandlewick_nb
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  if(BUILD_PINOCCHIO_VISUALIZER)
    target_compile_definitions(
      pycandlewick_nb
      PRIVATE CANDLEWICK_PYTHON_PINOCCHIO_SUPPORT
    )
  endif()
endfunction()

_add_nanobind_bindings()
