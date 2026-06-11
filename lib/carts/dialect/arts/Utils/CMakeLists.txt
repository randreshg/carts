add_mlir_dialect_library(MLIRCartsArtsUtils
  DbBackedMemrefUtils.cpp
  DbLayoutPlanUtils.cpp
  DbDistributedEligibility.cpp
  DbUtils.cpp
  EdtUtils.cpp
  LocationMetadata.cpp
  LoopStructureUtils.cpp
  LoweringFactUtils.cpp
  RuntimeConfig.cpp
  StringUtils.cpp
  ValueAnalysisUtils.cpp

  ADDITIONAL_HEADER_DIRS
    ${PROJECT_SOURCE_DIR}/include/carts/dialect/arts/Utils

  DEPENDS
    MLIRArtsOpsIncGen

  LINK_LIBS PUBLIC
    MLIRAffineDialect
    MLIRArithDialect
    MLIRFuncDialect
    MLIRIR
    MLIRLLVMDialect
    MLIRMemRefDialect
    MLIRSCFDialect
    MLIRSideEffectInterfaces
    MLIRTransformUtils
    MLIRCartsArts
)

target_link_libraries(MLIRCartsArtsUtils PRIVATE ${MLIR_POLYGEIST_LIB})
