# include("../.cmake/CMakeColors.txt")

# Install scripts.
# add_subdirectory(scripts)
set(CARTS_CXX_FLAGS 
  -O3
  -g0
  -fPIC
  -std=c++17
  -Wall
)

# Configure LLVM 
message_blue("Finding dependencies: ")
find_package(LLVM 14 REQUIRED CONFIG)

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_INSTALL_LIBDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_INSTALL_LIBDIR}/)

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})
link_directories(${LLVM_LIBRARY_DIRS})

message_green("Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
message_green("Found NOELLE ${NOELLE_VERSION} in ${NOELLE_INSTALL_DIR}")
# message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(AddLLVM)
include(HandleLLVMOptions)

include_directories(
  # LLVM
  ${LLVM_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}
  # External
  ${NOELLE_INSTALL_DIR}/include
)

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - #
# Sources
file(GLOB CARTSPassSrc "*.cpp")

# Compilation flags
set_source_files_properties(${CARTSPassSrc} PROPERTIES COMPILE_FLAGS " -std=c++17")

# Name of the LLVM pass
set(CARTSPassName "CARTS")

set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

# Declare the LLVM pass to compile
add_llvm_library(${CARTSPassName} MODULE ${CARTSPassSrc})