cmake_minimum_required(VERSION 3.0)
add_subdirectory(googletest)

file(GLOB SRCS ./*.cpp)

add_executable(UnitTest ${SRCS})
target_compile_options(UnitTest PUBLIC -std=c++17)
llvm_map_components_to_libnames(test_llvm core)
message(STATUS "Components mapped by llvm_config: ${test_llvm}")

target_include_directories(UnitTest
    PRIVATE
    .
    ${GTEST_DIR}/include
    ${FLEX_INCLUDE_CACHE}
    ${PARSER_HEADER_DIR}
    )

target_link_libraries(UnitTest
  PRIVATE
    gtest_main
    mimium_utils
    mimium_compiler
    ${test_llvm}
  )

add_test(Name UnitTest Command "ASAN_OPTIONS=detect_container_overflow=0 ${CMAKE_CURRENT_BINARY_DIR}/UnitTest")

file(GLOB_RECURSE testsource LIST_DIRECTORIES true mmm/*.mmm)


file(COPY ${testsource} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

add_subdirectory(regression)

add_subdirectory(fuzzing)