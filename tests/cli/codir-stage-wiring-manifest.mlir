// RUN: %carts-compile --print-pipeline-manifest-json | %FileCheck %s --check-prefix=MANIFEST --implicit-check-not='"currentStages"' --implicit-check-not='"targetStages"' --implicit-check-not='"arts-object-refinement"' --implicit-check-not='"arts-rt-lowering"'
// RUN: %carts-compile --print-pipeline-manifest-json | %FileCheck %s --check-prefix=DISTCANON
// RUN: not %carts-compile %s --pipeline arts-object-refinement -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-GROUP-ALIAS
// RUN: not %carts-compile %s --pipeline arts-rt-lowering -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-GROUP-ALIAS-RT
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(contraction-shortcut)' -o %t 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=NO-CONTRACTION-ALIAS

// Verifies production CODIR wiring: SDE planning no longer runs direct
// codelet lowering to ARTS, and executable `sde-planning` / `sde-to-codir` /
// `codir-graph-transforms` / `codir-to-arts` stages sit between frontend
// cleanup and ARTS refinement.

module {}

// MANIFEST: "sde-to-codir"
// MANIFEST: "codir-graph-transforms"
// MANIFEST: "codir-to-arts"
// MANIFEST: "name": "sde-planning"
// MANIFEST-NOT: "aliases"
// MANIFEST-NOT: "openmp-to-arts"
// MANIFEST-NOT: "ConvertSdeToArts"
// MANIFEST: "name": "sde-to-codir"
// MANIFEST: "ConvertSdeToCodir"
// MANIFEST-NOT: "CodirCodeletDCE"
// MANIFEST-SAME: "dependsOn": ["sde-planning"]
// MANIFEST: "name": "codir-graph-transforms"
// MANIFEST: "CodirCodeletDCE"
// MANIFEST: "ReductionDepMapping"
// MANIFEST: "ReductionAtomicMaterialization"
// MANIFEST: "DepStorageAssignment"
// MANIFEST: "VerifyCodir"
// MANIFEST-SAME: "dependsOn": ["sde-to-codir"]
// MANIFEST: "name": "codir-to-arts"
// MANIFEST-NOT: "ReductionDepMapping"
// MANIFEST-NOT: "ReductionAtomicMaterialization"
// MANIFEST-NOT: "DepStorageAssignment"
// MANIFEST: "ConvertSdeBoundaryToArts"
// MANIFEST: "ConvertCodirToArts"
// MANIFEST-NOT: "ConvertSdeToArts"
// MANIFEST-SAME: "dependsOn": ["codir-graph-transforms"]
// MANIFEST: "name": "edt-dep-realization"
// MANIFEST: "RealizeEdtDistributionPlan"
// MANIFEST: "VerifySdeLowered"
// MANIFEST: "VerifyArtsObjectsOnly"
// MANIFEST-SAME: "dependsOn": ["codir-to-arts"]
// MANIFEST: "name": "post-db-refinement"
// MANIFEST: "PartialReductionSplit"
// MANIFEST: "DbScratchElimination"
// MANIFEST: "CSE(arts.edt)"
// MANIFEST: "DistributedLaunchConsistency"
// MANIFEST-SAME: "dependsOn": ["create-dbs"]

// DISTCANON: "pipeline"
// DISTCANON: "epochs"
// DISTCANON: "VerifyArtsCdag"

// NO-GROUP-ALIAS: Unknown pipeline step: 'arts-object-refinement'

// NO-GROUP-ALIAS-RT: Unknown pipeline step: 'arts-rt-lowering'

// NO-CONTRACTION-ALIAS: 'contraction-shortcut' does not refer to a registered pass or pass pipeline
