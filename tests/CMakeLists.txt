find_package(GTest REQUIRED)

function(add_candlewick_test filename)
  cmake_path(GET filename STEM name)
  add_executable(${name} ${filename})
  target_link_libraries(${name} PRIVATE candlewick_core GTest::gtest_main)
  target_link_libraries(${name} PRIVATE ${ARGN})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_candlewick_test(TestMeshData.cpp)
add_candlewick_test(TestStrided.cpp)
