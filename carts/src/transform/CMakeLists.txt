file(GLOB CARTS_OMP_TRANSFORM_SOURCE_FILES "OMPTransform.cpp")

message_blue("Adding OMPTransformPass")

include_directories(${CLANG_INCLUDE_DIRS})

add_llvm_library( OMPTransform
  MODULE 
  OMPTransformPass.cpp
  ${CARTS_OMP_TRANSFORM_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
)
