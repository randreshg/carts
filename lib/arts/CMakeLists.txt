add_mlir_dialect_library(MLIRArts
  Dialect.cpp

  ADDITIONAL_HEADER_DIRS
    ${PROJECT_SOURCE_DIR}/include/carts

  DEPENDS
    MLIRArtsOpsIncGen

  LINK_LIBS PUBLIC
    MLIRIR
    MLIRMemRefDialect
    MLIRLLVMDialect
    MLIROpenMPDialect
    MLIRAffineDialect
    MLIRSupport
    MLIRSCFTransforms
)
add_subdirectory(utils)
add_subdirectory(transforms)
add_subdirectory(analysis)
add_subdirectory(codegen)
add_subdirectory(passes)