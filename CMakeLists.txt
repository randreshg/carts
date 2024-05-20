include("./.cmake/CMakeColors.txt")
cmake_minimum_required(VERSION 3.13)

project(CARTS C CXX)

set(LLVM_VERSION 14.0.6)
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

include(ExternalProject)
ExternalProject_Add(
    noelle
    GIT_REPOSITORY  "https://github.com/randreshg/noelle.git"
    GIT_TAG         v14
    BUILD_COMMAND   ${CMAKE_COMMAND} --build . -j16
    INSTALL_COMMAND ${CMAKE_COMMAND} --install .
    CMAKE_ARGS
        -DCMAKE_INSTALL_PREFIX=${NOELLE_INSTALL_DIR}
        -DNOELLE_SVF=OFF
        -DNOELLE_SCAF=ON
)
include_directories(${NOELLE_INSTALL_DIR}/include)

# Set up the build and install paths.
set(CMAKE_STAGING_PREFIX ${CMAKE_BINARY_DIR})
set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX}/lib)
set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX}/bin)
set(CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_PREFIX}/include)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_custom_target(
  CompileCommands ALL
  COMMAND ${CMAKE_COMMAND} -E create_symlink
    ${CMAKE_BINARY_DIR}/compile_commands.json
    ${CMAKE_SOURCE_DIR}/compile_commands.json
)

# configure_file(.cmake/enable.in enable @ONLY)

add_subdirectory(carts)