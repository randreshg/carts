add_mlir_dialect_library(MLIRArts
  dialect/core/IR/Dialect.cpp

  ADDITIONAL_HEADER_DIRS
    ${PROJECT_SOURCE_DIR}/include/arts

  DEPENDS
    MLIROpsIncGen
    MLIRRtOpsIncGen

  LINK_LIBS PUBLIC
    MLIRIR
    MLIRMemRefDialect
    MLIRLLVMDialect
    MLIROpenMPDialect
    MLIRAffineDialect
    MLIRLoopLikeInterface
    MLIRSupport
    MLIRSCFTransforms
)
add_subdirectory(dialect)
add_subdirectory(utils)
add_subdirectory(dialect/core/Transforms)
add_subdirectory(dialect/core/Conversion)
add_subdirectory(dialect/core/Analysis)
add_subdirectory(codegen)
add_subdirectory(passes)
