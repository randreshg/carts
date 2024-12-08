
message_blue("Adding OpenMPTool")
find_package(Clang REQUIRED CONFIG)

include_directories(${CLANG_INCLUDE_DIRS})
include_directories(${LLVM_INCLUDE_DIRS})

# Enable RTTI if disabled by default
if (NOT LLVM_ENABLE_RTTI)
    add_definitions(-fno-rtti)
endif()

# Add source files
set(SOURCES
    OpenMPConsumer.cpp
    OpenMPFrontend.cpp
)

# Add executable
add_executable(OpenMPTool ${SOURCES})

# Link against LLVM and Clang libraries
target_link_libraries(OpenMPTool
    PRIVATE
    clangTooling
    clangFrontend
    clangAST
    clangCodeGen
    clangBasic
    clangLex
    clangRewrite
    clangRewriteFrontend
    clangDriver
    clangSerialization
    LLVMSupport
    LLVMCore
)

# Add include directories for Clang and LLVM
target_include_directories(OpenMPTool PRIVATE ${CLANG_INCLUDE_DIRS})
target_include_directories(OpenMPTool PRIVATE ${LLVM_INCLUDE_DIRS})