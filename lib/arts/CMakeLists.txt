add_mlir_dialect_library(MLIRArts
  ArtsDialect.cpp

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
add_subdirectory(Utils)
add_subdirectory(Transforms)
add_subdirectory(Analysis)
add_subdirectory(Codegen)
add_subdirectory(Passes)