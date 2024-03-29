# Project
cmake_minimum_required(VERSION 3.4.3)
project(NOELLE-Tools)

# Programming languages to use
enable_language(C CXX)

# Find and link with LLVM
find_package(LLVM 9 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g -Weverything")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -Weverything")
set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

# Link with LLVM
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
#link_directories(${LLVM_LIBRARY_DIRS})

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)

# All include files for CPF
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${CMAKE_CURRENT_BINARY_DIR}/include
                    ${CMAKE_INSTALL_PREFIX}/include
                    ${CMAKE_INSTALL_PREFIX}/include/svf)

add_subdirectory(deadfunctioneliminator)
add_subdirectory(doall)
add_subdirectory(dswp)
add_subdirectory(enablers)
add_subdirectory(helix)
add_subdirectory(heuristics)
add_subdirectory(inliner)
add_subdirectory(loop_invariant_code_motion)
add_subdirectory(loop_metadata)
add_subdirectory(loop_stats)
add_subdirectory(parallelization_technique)
add_subdirectory(parallelizer)
add_subdirectory(pdg_stats)
add_subdirectory(privatizer)
add_subdirectory(scev_simplification)
add_subdirectory(codesize)
add_subdirectory(time_saved)
