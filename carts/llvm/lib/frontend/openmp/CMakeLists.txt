
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
    OMPConsumer.cpp
    OMPFrontend.cpp
)

# Add executable
# add_executable(OpenMPTool ${SOURCES})
add_library(OpenMPPlugin SHARED ${SOURCES})

# Link against LLVM and Clang libraries
target_link_libraries(OpenMPPlugin
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
target_include_directories(OpenMPPlugin PRIVATE ${CLANG_INCLUDE_DIRS})
target_include_directories(OpenMPPlugin PRIVATE ${LLVM_INCLUDE_DIRS})