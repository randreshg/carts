# add_llvm_loadable_module( OMPTransform
#   OMPTransform.cpp
#   PLUGIN_TOOL
#   opt
# )

add_llvm_library( OMPTransform
  MODULE 
  OMPTransform.cpp)
