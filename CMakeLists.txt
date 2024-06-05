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


# Install NOELLE
message_blue("Installing NOELLE.")
set(NOELLE_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/${NOELLE_DIR})
message_blue("NOELLE_BUILD_DIR: ${NOELLE_BUILD_DIR}")
configure_file(
  ${CMAKE_SOURCE_DIR}/.cmake/NoelleConfig.cmake.in
  ${NOELLE_BUILD_DIR}/CMakeLists.txt
)

execute_process(
  COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}"
        -D "CMAKE_MAKE_PROGRAM:FILE=${CMAKE_MAKE_PROGRAM}"
        .
  RESULT_VARIABLE NoelleInstallResult
  WORKING_DIRECTORY "${NOELLE_BUILD_DIR}"
)
if(NoelleInstallResult)
  error_red( "CMake step for NOELLE failed: ${NoelleInstallResult}")
endif()
execute_process(COMMAND ${CMAKE_COMMAND} --build .
                RESULT_VARIABLE NoelleInstallResult
                WORKING_DIRECTORY "${NOELLE_BUILD_DIR}"
)
if(NoelleInstallResult)
  error_red("Build step for NOELLE failed: ${NoelleInstallResult}")
endif()

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

# # configure_file(.cmake/enable.in enable @ONLY)
message_blue("Adding CARTS subdirectory...")

add_subdirectory(carts)