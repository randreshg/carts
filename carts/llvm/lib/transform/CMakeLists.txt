file(GLOB CARTS_OMP_TRANSFORM_SOURCE_FILES "ARTSTransform.cpp")

message_blue("Adding ARTSTransformPass")

include_directories(${CLANG_INCLUDE_DIRS})

add_llvm_library( ARTSTransform
  MODULE 
  ARTSTransformPass.cpp
  ${CARTS_OMP_TRANSFORM_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
)
