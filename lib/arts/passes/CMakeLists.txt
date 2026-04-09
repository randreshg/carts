add_mlir_dialect_library(MLIRArtsTransforms
  # Verification passes
  verify/VerifyMetadata.cpp
  verify/VerifyMetadataIntegrity.cpp
  verify/VerifyEdtCreated.cpp
  verify/VerifyEpochCreated.cpp
  verify/VerifyForLowered.cpp
  verify/VerifyDbCreated.cpp
  verify/VerifyPreLowered.cpp
  verify/VerifyLowered.cpp
  verify/VerifyEdtLowered.cpp
  verify/VerifyDbLowered.cpp
  verify/VerifyEpochLowered.cpp

  # Transformation passes
  transforms/CollectMetadata.cpp
  transforms/PatternDiscovery.cpp
  transforms/CreateDbs.cpp
  transforms/CreateEpochs.cpp
  transforms/Concurrency.cpp
  transforms/ConvertOpenMPToArts.cpp
  transforms/ConvertArtsToLLVM.cpp
  transforms/ConvertArtsToLLVMPatterns.cpp
  transforms/DbLowering.cpp
  transforms/EdtLowering.cpp
  transforms/EdtLoweringSupport.cpp
  transforms/EpochLowering.cpp
  transforms/LoweringContractCleanup.cpp
  transforms/ForOpt.cpp
  transforms/DistributedHostLoopOutlining.cpp
  transforms/EdtDistribution.cpp
  transforms/ForLowering.cpp
  transforms/ParallelEdtLowering.cpp
  transforms/EdtPtrRematerialization.cpp
  transforms/DepTransforms.cpp

  # Optimization passes
  opt/general/Inliner.cpp
  opt/general/RaiseMemRefDimensionality.cpp
  opt/general/HandleDeps.cpp
  opt/general/DeadCodeElimination.cpp
  opt/kernel/KernelTransforms.cpp
  opt/loop/LoopFusion.cpp
  opt/loop/LoopNormalization.cpp
  opt/loop/StencilBoundaryPeeling.cpp
  opt/loop/LoopReordering.cpp
  opt/db/BlockLoopStripMining.cpp
  opt/db/BlockLoopStripMiningSupport.cpp
  opt/db/ContractValidation.cpp
  opt/db/DbModeTightening.cpp
  opt/db/DbScratchElimination.cpp
  opt/db/DbPartitioning.cpp
  opt/db/DbPartitioningSupport.cpp
  opt/db/DbTransformsPass.cpp
  opt/db/DbDistributedOwnership.cpp
  opt/edt/EdtStructuralOpt.cpp
  opt/edt/EdtAllocaSinking.cpp
  opt/edt/EdtICM.cpp
  opt/edt/EdtTransformsPass.cpp
  opt/edt/EdtOrchestrationOpt.cpp
  # Epoch optimization split
  opt/epoch/EpochOpt.cpp
  opt/epoch/EpochOptSupport.cpp
  opt/epoch/EpochOptStructural.cpp
  opt/epoch/EpochOptScheduling.cpp
  opt/epoch/EpochOptCpsChain.cpp
  opt/edt/StructuredKernelPlanPass.cpp
  opt/edt/PersistentStructuredRegion.cpp
  opt/codegen/AliasScopeGen.cpp
  opt/codegen/Hoisting.cpp
  opt/codegen/DataPtrHoisting.cpp
  opt/codegen/DataPtrHoistingSupport.cpp
  opt/codegen/GuidRangCallOpt.cpp
  opt/codegen/RuntimeCallOpt.cpp
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
