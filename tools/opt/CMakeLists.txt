get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)
set(LIBS
  ${dialect_libs}
  ${conversion_libs}
  MLIROptLib
  MLIRArts
  MLIRArtsTransforms
  MLIRFuncAllExtensions
  MLIRPolygeist
  MLIRPolygeistTransforms
  MLIRGPUToNVVMTransforms
  MLIRGPUToROCDLTransforms
  MLIRGPUToGPURuntimeTransforms
  MLIRGPUTransforms
)

add_llvm_executable(carts-opt carts-opt.cpp)

install(TARGETS carts-opt
  EXPORT CartsTargets
  RUNTIME DESTINATION ${CARTS_TOOLS_DIR}
  COMPONENT carts-opt)

llvm_update_compile_flags(carts-opt)
target_link_libraries(carts-opt PRIVATE ${LIBS})
