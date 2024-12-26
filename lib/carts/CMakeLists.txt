add_mlir_dialect_library(MLIRarts
  Dialect.cpp
  Ops.cpp

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

# add_subdirectory(Passes)
