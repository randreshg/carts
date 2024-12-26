include("./_cmake/CMakeColors.txt")
cmake_minimum_required(VERSION 3.13.4)

include(CheckCXXSourceCompiles)

if(POLICY CMP0068)
  cmake_policy(SET CMP0068 NEW)
  set(CMAKE_BUILD_WITH_INSTALL_NAME_DIR ON)
endif()

if(POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

if(POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

option(LLVM_INCLUDE_TOOLS "Generate build targets for the LLVM tools." ON)
option(LLVM_BUILD_TOOLS "Build the LLVM tools. If OFF, just generate build targets." ON)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/lib)

project(CARTS LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")

message_title("Project CARTS")

# Building the project against a build area.
message_blue("Searching for MLIRConfig.cmake in: ${MLIR_DIR}")
find_package(MLIR REQUIRED CONFIG)

set(Clang_DIR ${CLANG_DIR})
message_blue("Searching for ClangConfig.cmake in: ${Clang_DIR}")

if(NOT LLVM_BUILD_MAIN_SRC_DIR)
    set(LLVM_BUILD_MAIN_SRC_DIR ${CMAKE_SOURCE_DIR}/llvm-project/llvm)
  endif()
set(LLVM_SOURCE_DIR ${LLVM_BUILD_MAIN_SRC_DIR} CACHE STRING "Location of LLVM source")

# Status
message_info("Using MLIRConfig.cmake in: ${MLIR_DIR}")
message_info("Using LLVMConfig.cmake in: ${LLVM_DIR}")
message_info("Using ClangConfig.cmake in: ${CLANG_DIR}")

# Set up the build and install paths.
list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CLANG_CMAKE_DIR}")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}")
include(TableGen)
include(AddLLVM)
include(AddMLIR)
include(AddClang)
include(HandleLLVMOptions)

include_directories(${LLVM_INCLUDE_DIRS})
include_directories(${CLANG_INCLUDE_DIRS})
include_directories(${MLIR_INCLUDE_DIRS})
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_BINARY_DIR}/include)
link_directories(${LLVM_BUILD_LIBRARY_DIR})
add_definitions(${LLVM_DEFINITIONS})

# set(CARTS_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(CARTS_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})
set(CARTS_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/include )
set(CARTS_TOOLS_DIR ${CMAKE_BINARY_DIR}/bin)
set(CARTS_UTILS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/utils)

# Add the subdirectories.
message_blue("Adding lib and include subdirectories...")
add_subdirectory(include)
add_subdirectory(lib)