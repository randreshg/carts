// RUN: %carts-compile %S/../inputs/snapshots/2mm_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=TWOMM
// RUN: %carts-compile %S/../inputs/snapshots/3mm_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=THREEMM

// Without loop metadata, all read-only inputs stay coarse-allocated with
// uniform distribution since the owner dimension cannot be determined from
// metadata alone. The pattern pipeline classifies them later at the
// acquire level.

// --- 2MM: D = alpha*A*B*C + beta*D ---
// All read inputs are coarse-allocated.
// TWOMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// TWOMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// TWOMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]

// --- 3MM: E = A*B, F = C*D, G = E*F ---
// All read inputs are coarse-allocated.
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]

module {
}
