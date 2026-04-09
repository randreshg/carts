add_mlir_dialect_library(MLIRArtsRt
  RtDialect.cpp
  RtOps.cpp

  ADDITIONAL_HEADER_DIRS
    ${PROJECT_SOURCE_DIR}/include/arts/dialect/rt/IR

  DEPENDS
    MLIRRtOpsIncGen

  LINK_LIBS PUBLIC
    MLIRArithDialect
    MLIRIR
    MLIRMemRefDialect
)
