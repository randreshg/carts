
message_blue("Adding OMPTransform module")
add_llvm_library( OMPTransform
  MODULE 
  OMPTransform.cpp
  ${CARTS_ANALYSIS_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
  )
