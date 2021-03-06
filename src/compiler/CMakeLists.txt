include(AddFlexBisonDeps)

find_package(SndFile REQUIRED)

add_library(mimium_builtinfn SHARED ffi.cpp)
target_include_directories(mimium_builtinfn
PUBLIC
${SNDFILE_INCLUDE_DIRS}
)
target_link_libraries(mimium_builtinfn PUBLIC mimium_utils  ${SNDFILE_LIBRARIES})


add_subdirectory(codegen)
add_library(mimium_parser ${FLEX_MyScanner_OUTPUTS} ${BISON_MyParser_OUTPUTS} ast_loader.cpp  )
target_compile_options(mimium_parser PUBLIC -std=c++17)


set(PARSER_HEADER_DIR ${CMAKE_CURRENT_BINARY_DIR} CACHE PATH "for mimium_parser.hpp ")# for location.hh 
set(FLEX_INCLUDE_CACHE ${FLEX_INCLUDE_DIRS} CACHE PATH "to build test")
target_include_directories(mimium_parser
PUBLIC
${MIMIUM_SOURCE_DIR}
${FLEX_INCLUDE_CACHE}
${PARSER_HEADER_DIR} 
)

add_library(mimium_compiler symbolrenamer.cpp mirgenerator.cpp type_infer_visitor.cpp closure_convert.cpp collect_memoryobjs.cpp compiler.cpp)
add_dependencies(mimium_compiler  mimium_parser mimium_builtinfn)
target_include_directories(mimium_compiler
PUBLIC
${MIMIUM_SOURCE_DIR}
)
target_compile_options(mimium_compiler PUBLIC -std=c++17)

target_link_libraries(mimium_compiler PUBLIC mimium_parser mimium_llvm_codegen mimium_utils mimium_builtinfn)

install(TARGETS mimium_compiler mimium_builtinfn DESTINATION lib)