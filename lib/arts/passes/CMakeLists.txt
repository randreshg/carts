# Print it here
message(STATUS "POLYGEIST_INCLUDE_DIR: ${POLYGEIST_BUILD_DIR}/include")
message(STATUS "POLYGEIST_LIBRARY_DIR: ${POLYGEIST_BUILD_DIR}/lib")

add_mlir_dialect_library(MLIRArtsTransforms
  # Transformation passes
  transforms/CollectMetadata.cpp
  transforms/VerifyMetadata.cpp
  transforms/PatternDiscovery.cpp
  transforms/CreateDbs.cpp
  transforms/CreateEpochs.cpp
  transforms/Concurrency.cpp
  transforms/ConvertOpenMPToArts.cpp
  transforms/ConvertArtsToLLVM.cpp
  transforms/DbLowering.cpp
  transforms/EdtLowering.cpp
  transforms/EpochLowering.cpp
  transforms/LoweringContractCleanup.cpp
  transforms/ForOpt.cpp
  transforms/DistributedHostLoopOutlining.cpp
  transforms/EdtDistribution.cpp
  transforms/ForLowering.cpp
  transforms/ParallelEdtLowering.cpp
  transforms/LoopReordering.cpp
  transforms/EdtPtrRematerialization.cpp
  
  # Optimization passes
  opt/general/Inliner.cpp
  opt/general/CanonicalizeMemrefs.cpp
  opt/general/DeadCodeElimination.cpp
  opt/dep/DepTransforms.cpp
  opt/kernel/KernelTransforms.cpp
  opt/loop/LoopFusion.cpp
  opt/loop/LoopNormalization.cpp
  opt/loop/StencilBoundaryPeeling.cpp
  opt/db/BlockLoopStripMining.cpp
  opt/db/ContractValidation.cpp
  opt/db/DbModeTightening.cpp
  opt/db/DbScratchElimination.cpp
  opt/db/DbPartitioning.cpp
  opt/db/DbTransformsPass.cpp
  opt/db/DbDistributedOwnership.cpp
  opt/edt/EdtStructuralOpt.cpp
  opt/edt/EdtICM.cpp
  opt/edt/EdtTransformsPass.cpp
  opt/edt/EpochOpt.cpp
  opt/codegen/AliasScopeGen.cpp
  opt/codegen/Hoisting.cpp
  opt/codegen/DataPtrHoisting.cpp
  opt/codegen/LoopVectorizationHints.cpp
  opt/codegen/ScalarReplacement.cpp
  
  ${CARTS_TRANSFORMS_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
  ${CARTS_CODEGEN_SOURCE_FILES}
  ${CARTS_ANALYSIS_SOURCE_FILES}

DEPENDS
  MLIROpsIncGen
  MLIRArtsPassIncGen

LINK_LIBS PUBLIC
  MLIRAffineDialect
  MLIRArithDialect
  MLIRAsyncDialect
  MLIRAffineUtils
  MLIRFuncDialect
  MLIRFuncTransforms
  MLIRLLVMDialect
  MLIRMathDialect
  MLIRMathToLLVM
  MLIRMemRefDialect
  MLIRNVVMDialect
  MLIRPass
  MLIRArts
  MLIRSideEffectInterfaces
  MLIRSCFToControlFlow
  MLIRTargetLLVMIRImport
  MLIRTransformUtils
  MLIRControlFlowToLLVM
  MLIRMemRefToLLVM
  MLIRFuncToLLVM
  MLIRArithToLLVM
  MLIROpenMPToLLVM
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(MLIRArtsTransforms PUBLIC stdc++fs)
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7)
    target_link_libraries(MLIRArtsTransforms PUBLIC c++experimental)
  elseif(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(MLIRArtsTransforms PUBLIC c++fs)
  endif()
endif()

# Find Polygeist lib
target_link_libraries(MLIRArtsTransforms PRIVATE ${MLIR_POLYGEIST_LIB})

target_compile_definitions(obj.MLIRArtsTransforms
  PRIVATE
  CARTS_PGO_DEFAULT_DATA_DIR="${CARTS_PGO_DEFAULT_DATA_DIR}"
  CARTS_PGO_ALTERNATIVE_ENV_VAR="${CARTS_PGO_ALTERNATIVE_ENV_VAR}"
  CARTS_PGO_DATA_DIR_ENV_VAR="${CARTS_PGO_DATA_DIR_ENV_VAR}"
)
