// RUN: not %carts-compile %s --arts-config %S/../../examples/arts.cfg --verify-metadata-integrity 2>&1 | %FileCheck %s

// CHECK: duplicates metadata arts.id=21

module {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index

    arts.for(%c0) to(%c4) step(%c1) {{
    ^bb0(%iv: index):
      arts.yield
    }} {arts.id = 21 : i64, arts.metadata_provenance = "exact", arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "metadata_fixture.c:13:3">}

    arts.for(%c0) to(%c4) step(%c1) {{
    ^bb0(%iv: index):
      arts.yield
    }} {arts.id = 21 : i64, arts.metadata_provenance = "exact", arts.loop = #arts.loop_metadata<potentiallyParallel = true, hasReductions = false, tripCount = 4 : i64, nestingLevel = 0 : i64, hasInterIterationDeps = false, memrefsWithLoopCarriedDeps = 0 : i64, parallelClassification = 2 : i64, locationKey = "metadata_fixture.c:19:3">}

    return
  }
}
