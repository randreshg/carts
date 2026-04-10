// RUN: %carts-compile %S/../inputs/snapshots/2mm_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=TWOMM
// RUN: %carts-compile %S/../inputs/snapshots/3mm_openmp_to_arts.mlir --pipeline db-partitioning --arts-config %S/../inputs/arts_64t.cfg | %FileCheck %s --check-prefix=THREEMM

// Matmul reduction inputs that do not follow the distributed owner dimension
// must remain whole-range after db-partitioning. In C = A*B distributed by
// rows, A follows the owner dimension (row-partitioned, <block>) while B is
// accessed along the reduction axis and must stay coarse (<coarse>).

// --- 2MM: D = alpha*A*B*C + beta*D ---
// Two matmul phases: tmp = A*B, then D += tmp*C.
// A follows the owner dimension and stays block-partitioned.
// B is the reduction input for the first phase, C for the second.
// Both reduction inputs must be allocated as <coarse> (whole-range).
// TWOMM: arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>]
// TWOMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// TWOMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]

// --- 3MM: E = A*B, F = C*D, G = E*F ---
// Three matmul phases. A and C follow the owner dimension (block).
// B is the reduction input for E=A*B, D for F=C*D.
// Both reduction inputs must be allocated as <coarse> (whole-range).
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <block>, <indexed>]
// THREEMM-DAG: arts.db_alloc[<in>, <heap>, <read>, <coarse>, <uniform>]

module {
}
