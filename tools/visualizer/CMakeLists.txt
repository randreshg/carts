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
  MLIRSupport
  MLIRIR
  MLIRAnalysis
)

add_llvm_executable(carts-visualizer carts-visualizer.cpp)

install(TARGETS carts-visualizer
  EXPORT CartsTargets
  RUNTIME DESTINATION ${LLVM_TOOLS_INSTALL_DIR}
  COMPONENT carts-visualizer)

llvm_update_compile_flags(carts-visualizer)
target_link_libraries(carts-visualizer PRIVATE ${LIBS})

