cmake_minimum_required(VERSION 3.30)
project(kernel_instrument)

find_package(LLVM 14 REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

set(CMAKE_CXX_STANDARD 20)

add_library(kernel_instrument SHARED
        main.cpp
)
set_target_properties(kernel_instrument PROPERTIES
        OUTPUT_NAME "pass"
        PREFIX ""
)

add_custom_target(run_tests
        "${PROJECT_SOURCE_DIR}/test.sh" "${PROJECT_SOURCE_DIR}/test-ir" "${PROJECT_SOURCE_DIR}/test-results" "${PROJECT_BINARY_DIR}/pass.so"
        DEPENDS kernel_instrument
)
