// RUN: not %carts-compile %s --pass-pipeline='builtin.module(db-owner-map-realization)' 2>&1 \
// RUN:   | %FileCheck %s

// Owner-map realization is staged-only: it needs the ARTS AnalysisManager and
// runtime configuration, so it is not exposed as a textual pass. Confirm the
// current pass name cannot be run through a textual --pass-pipeline.

module {
  func.func @owner_map_realization_requires_staged_pipeline() {
    return
  }
}

// CHECK: 'db-owner-map-realization' does not refer to a registered pass or pass pipeline
