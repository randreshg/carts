file(GLOB CARTS_OMP_TRANSFORM_SOURCE_FILES "OMPTransform.cpp")

message_blue("Adding OMPTransformPass")

include_directories(${CLANG_INCLUDE_DIRS})

add_llvm_library( OMPTransform
  MODULE 
  OMPTransformPass.cpp
  visitor/OMPVisitor.cpp
  ${CARTS_OMP_TRANSFORM_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
  ${CLANG_LIBS}
  DEPENDS
  intrinsics_gen
)

# Link against Clang libraries found by find_package(Clang)
target_link_libraries(OMPTransform
  PRIVATE
  clangAST
  clangBasic
  clangFrontend
  clangSerialization
  clangLex
  clangSema
  clangToolingCore
)