add_subdirectory(Verify)

set(CARTS_SDE_TRANSFORMS_SOURCE_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/state/SdeLoopPatternFacts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/MemoryUnitRealization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/RankExpandMu.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/CuNormalization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/ScalarBlockReduction.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/RaiseToMuAccessWindow.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/CoarseAvoidance.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/Redistribute.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/PromoteTargetAttrs.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/ScalarForwarding.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/state/DeadStateCleanup.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/loop/Parallelize.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/loop/LayoutAssignment.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/loop/IterationSpaceDecomposition.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/loop/Interchange.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/loop/Tiling.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/dep/fusion/ElementwiseFusion.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/scheduling/ChunkOpt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/scheduling/ReductionStrategy.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/scheduling/AtomicReductionRealization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/scheduling/ScheduleRefinement.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/distribution/BarrierElimination.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/distribution/MuAccessWindowSyncOpt.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/effect/distribution/DistributionPlanning.cpp"
)
set(CARTS_SDE_TRANSFORMS_SOURCE_FILES ${CARTS_SDE_TRANSFORMS_SOURCE_FILES} PARENT_SCOPE)
set(CARTS_SDE_VERIFY_SOURCE_FILES ${CARTS_SDE_VERIFY_SOURCE_FILES} PARENT_SCOPE)
