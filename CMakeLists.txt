cmake_minimum_required(VERSION 3.30)
project(llvm-ptr-tracker)

find_package(LLVM 14 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

set(CMAKE_CXX_STANDARD 20)

add_library(ir_instrument SHARED
        instrument_ir.cpp
        struct_filter.cpp
        struct_filter.h
        util.cpp
        util.h
)
set_target_properties(ir_instrument PROPERTIES
        OUTPUT_NAME "ir_instr"
        PREFIX ""
)

add_library(purge_stores SHARED
    purge_stores.cpp
)
set_target_properties(purge_stores PROPERTIES
        OUTPUT_NAME "purge_stores"
        PREFIX ""
)

add_custom_target(run_tests
        "${PROJECT_SOURCE_DIR}/tests/test.sh" "${PROJECT_BINARY_DIR}/ir_instr.so"
        DEPENDS ir_instrument
        DEPENDS purge_stores
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}/tests"
)
