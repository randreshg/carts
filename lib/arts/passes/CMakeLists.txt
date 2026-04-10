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
  verify/VerifySdeLowered.cpp
  verify/VerifyEdtLowered.cpp
  verify/VerifyDbLowered.cpp
  verify/VerifyEpochLowered.cpp

  # SDE dialect passes
  ../dialect/sde/Transforms/CollectMetadata.cpp
  ../dialect/sde/Transforms/ConvertOpenMPToSde.cpp
  ../dialect/sde/Transforms/RaiseToLinalg.cpp
  # Core dialect passes — conversion
  ../dialect/core/Conversion/SdeToArts/SdeToArtsPatterns.cpp
  ../dialect/core/Conversion/OmpToArts/ConvertOpenMPToArts.cpp
  ../dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVM.cpp
  ../dialect/core/Conversion/ArtsToLLVM/ConvertArtsToLLVMPatterns.cpp

  # Core dialect passes — transforms
  ../dialect/core/Transforms/Concurrency.cpp
  ../dialect/core/Transforms/CreateDbs.cpp
  ../dialect/core/Transforms/CreateEpochs.cpp
  ../dialect/core/Transforms/DbLowering.cpp
  ../dialect/core/Transforms/ParallelEdtLowering.cpp
  ../dialect/core/Transforms/ForLowering.cpp
  ../dialect/core/Transforms/ForOpt.cpp
  ../dialect/core/Transforms/DistributedHostLoopOutlining.cpp
  ../dialect/core/Transforms/EdtDistribution.cpp
  ../dialect/core/Transforms/EdtPtrRematerialization.cpp
  ../dialect/core/Transforms/DepTransforms.cpp
  ../dialect/core/Transforms/PatternDiscovery.cpp
  ../dialect/core/Transforms/KernelTransforms.cpp
  ../dialect/core/Transforms/StencilBoundaryPeeling.cpp
  ../dialect/core/Transforms/LoweringContractCleanup.cpp
  # General transforms (ARTS-aware)
  ../dialect/core/Transforms/Inliner.cpp
  ../dialect/core/Transforms/RaiseMemRefDimensionality.cpp
  ../dialect/core/Transforms/HandleDeps.cpp
  ../dialect/core/Transforms/DeadCodeElimination.cpp
  ../dialect/core/Transforms/ScalarReplacement.cpp
  # Loop transforms
  ../dialect/core/Transforms/LoopFusion.cpp
  ../dialect/core/Transforms/LoopNormalization.cpp
  ../dialect/core/Transforms/LoopReordering.cpp
  # DB optimization
  ../dialect/core/Transforms/BlockLoopStripMining.cpp
  ../dialect/core/Transforms/BlockLoopStripMiningSupport.cpp
  ../dialect/core/Transforms/ContractValidation.cpp
  ../dialect/core/Transforms/DbModeTightening.cpp
  ../dialect/core/Transforms/DbScratchElimination.cpp
  ../dialect/core/Transforms/DbPartitioning.cpp
  ../dialect/core/Transforms/DbPartitioningSupport.cpp
  ../dialect/core/Transforms/DbTransformsPass.cpp
  ../dialect/core/Transforms/DbDistributedOwnership.cpp
  # EDT optimization
  ../dialect/core/Transforms/EdtStructuralOpt.cpp
  ../dialect/core/Transforms/EdtAllocaSinking.cpp
  ../dialect/core/Transforms/EdtICM.cpp
  ../dialect/core/Transforms/EdtTransformsPass.cpp
  ../dialect/core/Transforms/EdtOrchestrationOpt.cpp
  ../dialect/core/Transforms/StructuredKernelPlanPass.cpp
  ../dialect/core/Transforms/PersistentStructuredRegion.cpp
  # Epoch optimization
  ../dialect/core/Transforms/EpochOpt.cpp
  ../dialect/core/Transforms/EpochOptSupport.cpp
  ../dialect/core/Transforms/EpochOptStructural.cpp
  ../dialect/core/Transforms/EpochOptScheduling.cpp
  ../dialect/core/Transforms/EpochOptCpsChain.cpp
  # Codegen
  ../dialect/core/Transforms/AliasScopeGen.cpp
  ../dialect/core/Transforms/Hoisting.cpp
  ../dialect/core/Transforms/LoopVectorizationHints.cpp

  # Core dialect passes — ArtsToRt conversion
  ../dialect/core/Conversion/ArtsToRt/EdtLowering.cpp
  ../dialect/core/Conversion/ArtsToRt/EdtLoweringSupport.cpp
  ../dialect/core/Conversion/ArtsToRt/EpochLowering.cpp

  # RT dialect passes
  ../dialect/rt/Conversion/RtToLLVM/RtToLLVMPatterns.cpp
  ../dialect/rt/Transforms/DataPtrHoisting.cpp
  ../dialect/rt/Transforms/DataPtrHoistingSupport.cpp
  ../dialect/rt/Transforms/GuidRangCallOpt.cpp
  ../dialect/rt/Transforms/RuntimeCallOpt.cpp
  
  ${CARTS_TRANSFORMS_SOURCE_FILES}
  ${CARTS_UTILS_SOURCE_FILES}
  ${CARTS_CODEGEN_SOURCE_FILES}
  ${CARTS_ANALYSIS_SOURCE_FILES}

DEPENDS
  MLIROpsIncGen
  MLIRArtsPassIncGen
  MLIRArtsRtPassIncGen
  MLIRArtsSdePassIncGen
  MLIRRtOpsIncGen
  MLIRSdeOpsIncGen

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
  MLIRArtsRt
  MLIRArtsSde
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
