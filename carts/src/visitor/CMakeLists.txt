
message_blue("Adding OMPVisitorPass")
include_directories(${CLANG_INCLUDE_DIRS})

add_llvm_library( OMPVisitor
  MODULE
  OMPVisitor.cpp
  ${CARTS_UTILS_SOURCE_FILES}
  ${CLANG_LIBS}
  DEPENDS
  intrinsics_gen
)

# Link against Clang libraries found by find_package(Clang)
target_link_libraries(OMPVisitor
  PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangSerialization
  clangLex
  clangSema
  clangToolingCore
)