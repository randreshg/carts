// RUN: %carts-compile --O1 --print-pipeline-manifest-json | %FileCheck %s --check-prefixes=PIPE,O1
// RUN: %carts-compile --O2 --print-pipeline-manifest-json | %FileCheck %s --check-prefixes=PIPE,O2
// RUN: %carts-compile --O3 --print-pipeline-manifest-json | %FileCheck %s --check-prefixes=PIPE,O3
// RUN: %carts-compile --O0 --print-pipeline-manifest-json | %FileCheck %s --check-prefix=O0
// RUN: not %carts-compile %s --O0 --pipeline=sde-planning 2>&1 | %FileCheck %s --check-prefix=O0ERR

module {}

// PIPE:      {"name": "sde-planning"
// PIPE-SAME: "SdeCuNormalization"
// PIPE-SAME: "SdeRankExpandMu"
// PIPE-SAME: "RaiseToMuAccessWindow"
// PIPE-SAME: "MuAccessWindowSyncOpt"
// PIPE-SAME: "SdeRedistribute"
// PIPE-SAME: "SdeCoarseAvoidance"
// PIPE-SAME: "VerifySde"
// PIPE-SAME: "dependsOn": ["initial-cleanup"]
// PIPE:      {"name": "sde-to-arts"
// PIPE-SAME: "SdeStorageToArtsDb"
// PIPE-SAME: "SdeAccessesToArtsDeps"
// PIPE-SAME: "FinalizeSdeToArts"
// PIPE-SAME: "dependsOn": ["sde-planning"]

// O1: "O1": {"pipeline_sequence": ["sde-input-normalization"
// O1-SAME: "sde-planning"
// O1-SAME: "sde-to-arts"

// O2: "O2": {"pipeline_sequence": ["sde-input-normalization"
// O2-SAME: "sde-planning"
// O2-SAME: "sde-to-arts"

// O3: "O3": {"pipeline_sequence": ["sde-input-normalization"
// O3-SAME: "sde-planning"
// O3-SAME: "sde-to-arts"
// O3-SAME: "epilogue_sequence": ["post-o3-opt"]

// O0: "O0": {"pipeline_sequence": [], "epilogue_sequence": []}

// O0ERR: -O0 does not run the staged CARTS optimization pipeline
