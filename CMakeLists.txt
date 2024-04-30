include("./.cmake/CMakeColors.txt")
cmake_minimum_required(VERSION 3.13)

project(CARTS C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS OFF)

set(LLVM_VERSION 14.0.1)
set(CLANG_ID "Clang")

message_blue("Compiling CARTS for LLVM version ${LLVM_VERSION}")

# Check that we are compiling with clang.
if (NOT CMAKE_C_COMPILER_ID STREQUAL CLANG_ID)
  error_red(
    "C compiler ID is not clang!\n"
    "    Found compiler id ${CMAKE_CXX_COMPILER_ID}."
    " Ensure that your PATH is set correctly."
    " If it is not installed, do so at: https://releases.llvm.org/${LLVM_VERSION}/")
endif()

if (NOT CMAKE_CXX_COMPILER_ID STREQUAL CLANG_ID)
  error_red(
    "C++ compiler ID is not clang!"
    "    Found compiler id ${CMAKE_CXX_COMPILER_ID}."
    " Ensure that your PATH is set correctly."
    " If it is not installed, do so at: https://releases.llvm.org/${LLVM_VERSION}/")
endif()


# Check that clang/clang++ we are using is the correct version.
if (NOT CMAKE_C_COMPILER_VERSION VERSION_EQUAL LLVM_VERSION)
  error_red(
    "Clang version is not ${LLVM_VERSION}!"
    "    Found version ${CMAKE_C_COMPILER_VERSION}."
    " Ensure that your PATH is set correctly."
    " If it is not installed, do so at: https://releases.llvm.org/${LLVM_VERSION}/")
endif()

if (NOT CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL LLVM_VERSION)
  error_red(
    "Clang++ version is not ${LLVM_VERSION}!"
    "    Found version ${CMAKE_CXX_COMPILER_VERSION}"
    " Ensure that your PATH is set correctly."
    " If it is not installed, do so at: https://releases.llvm.org/${LLVM_VERSION}/")
endif()

if (NOT NOELLE_INSTALL_DIR)
  error_red(
    "NOELLE_INSTALL_DIR is not set!"
    "    Please set NOELLE_INSTALL_DIR to the path where NOELLE is installed.")
endif()

# Set up the build and install paths.
set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR})
set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(src)