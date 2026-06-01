// RUN: %carts-compile --print-pipeline-manifest-json | %FileCheck %s --check-prefix=MANIFEST --implicit-check-not='"currentStages"' --implicit-check-not='"targetStages"' --implicit-check-not='"arts-object-refinement"' --implicit-check-not='"arts-rt-lowering"'
// RUN: %carts-compile --distributed-db --print-pipeline-manifest-json | %FileCheck %s --check-prefix=DISTCANON
// RUN: not %carts-compile --distributed-dbs --print-pipeline-manifest-json 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NOALIAS
// RUN: not %carts-compile %s --pipeline arts-object-refinement -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-GROUP-ALIAS
// RUN: not %carts-compile %s --pipeline arts-rt-lowering -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-GROUP-ALIAS-RT
// RUN: not %carts-compile %s --distributed-db --pass-pipeline='builtin.module(canonicalize)' -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=PASSPIPE-DIST
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(matmul-3mm-contraction-materialization)' -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-MATMUL-3MM

// Verifies production CODIR wiring: SDE planning no longer runs direct
// codelet lowering to ARTS, and executable `sde-planning` / `sde-to-codir` /
// `codir-to-arts` stages sit between frontend cleanup and ARTS refinement.

module {}

// MANIFEST: "sde-to-codir"
// MANIFEST: "codir-to-arts"
// MANIFEST: "name": "sde-planning"
// MANIFEST-NOT: "aliases"
// MANIFEST-NOT: "openmp-to-arts"
// MANIFEST-NOT: "ConvertSdeToArts"
// MANIFEST: "name": "sde-to-codir"
// MANIFEST: "ConvertSdeToCodir"
// MANIFEST: "CodirCodeletOpt"
// MANIFEST: "ReductionPlanning"
// MANIFEST: "StoragePlanning"
// MANIFEST: "VerifyCodir"
// MANIFEST-SAME: "dependsOn": ["sde-planning"]
// MANIFEST: "name": "codir-to-arts"
// MANIFEST-NOT: "ReductionPlanning"
// MANIFEST-NOT: "StoragePlanning"
// MANIFEST: "MaterializeSdeBoundaryToArts"
// MANIFEST: "ConvertCodirToArts"
// MANIFEST-NOT: "ConvertSdeToArts"
// MANIFEST: "VerifySdeLowered"
// MANIFEST: "VerifyArtsObjectsOnly"
// MANIFEST-SAME: "dependsOn": ["sde-to-codir"]
// MANIFEST: "name": "post-db-refinement"
// MANIFEST: "PartialReductionSplitMaterialization"
// MANIFEST: "DbScratchElimination"
// MANIFEST: "CSE(arts.edt)"
// MANIFEST: "DistributedLaunchConsistency"
// MANIFEST-SAME: "dependsOn": ["create-dbs"]

// DISTCANON: "pipeline"
// DISTCANON: "post-db-refinement"
// DISTCANON: "VerifyDistributedDbPlacement"

// NOALIAS: Unknown command line argument '--distributed-dbs'
// NOALIAS: Did you mean '--distributed-db'?

// NO-GROUP-ALIAS: Unknown pipeline step: 'arts-object-refinement'

// NO-GROUP-ALIAS-RT: Unknown pipeline step: 'arts-rt-lowering'

// PASSPIPE-DIST: --distributed-db requires the staged CARTS pipeline
// PASSPIPE-DIST-SAME: --pass-pipeline
// PASSPIPE-DIST-SAME: DbDistributedOwnership
// PASSPIPE-DIST-SAME: VerifyDistributedDbPlacement

// NO-MATMUL-3MM: 'matmul-3mm-contraction-materialization' does not refer to a registered pass or pass pipeline
