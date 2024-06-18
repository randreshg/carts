
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

message_green("Found LLVM ${LLVM_PACKAGE_VERSION} in ${LLVM_DIR}")
message_green("Found NOELLE ${NOELLE_VERSION} in ${NOELLE_INSTALL_DIR}")

# Prepare the pass to be included in the source tree
list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions)
include(AddLLVM)

add_definitions(${LLVM_DEFINITIONS})
include_directories(
  # LLVM
  ${LLVM_INCLUDE_DIRS}
  # Include files
  ./include
)
link_directories(${LLVM_LIBRARY_DIRS})
# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - #
# Sources
# file(GLOB CARTSPassSrc "*.cpp" "src/*.cpp")

# Compilation flags
add_compile_options(${CARTS_CXX_FLAGS})
# set_source_files_properties(${CARTSPassSrc} PROPERTIES COMPILE_FLAGS)
# set_source_files_properties(${CARTSPassSrc} PROPERTIES COMPILE_FLAGS " -std=c++17")


# Name of the LLVM pass
# set(CARTSPassName "CARTS")

# set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)
# set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}/${CMAKE_CFG_INTDIR}/)

# Add src directory
add_subdirectory(src)