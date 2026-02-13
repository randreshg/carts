// RUN: %carts-compile %S/../examples/addition/addition.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at pre-lowering | %FileCheck %s --check-prefix=UNIFORM
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at pre-lowering | %FileCheck %s --check-prefix=STENCIL
// RUN: %carts-compile %S/../examples/matrixmul/matrixmul.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at pre-lowering | %FileCheck %s --check-prefix=MATMUL

// Contract: every lowered task creation path must record dependencies through
// arts.rec_dep (which is then lowered to runtime dependency APIs).

// UNIFORM: arts.edt_create
// UNIFORM: arts.rec_dep
// UNIFORM-SAME: acquire_modes =

// STENCIL: arts.edt_create
// STENCIL: arts.rec_dep
// STENCIL-SAME: acquire_modes =

// MATMUL: arts.edt_create
// MATMUL: arts.rec_dep
// MATMUL-SAME: acquire_modes =

module {
  // No IR body needed: this file only carries RUN/FileCheck contracts.
}
