// RUN: %carts-compile %S/../../external/carts-benchmarks/graph500/graph-gen/graph_gen.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at openmp-to-arts | %FileCheck %s --check-prefix=OMP
// RUN: %carts-compile %S/../../external/carts-benchmarks/graph500/graph-gen/graph_gen.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at epochs | %FileCheck %s --check-prefix=EPOCH
// RUN: %carts-compile %S/../../external/carts-benchmarks/graph500/graph-gen/graph_gen.mlir --O3 --arts-config %S/../examples/arts.cfg --emit-llvm | %FileCheck %s --check-prefix=LLVM

// Contract: benchmark E2E timers must stay outside the outlined ARTS work.
// The stop timer must remain ordered after the OpenMP join/barrier, after
// epoch materialization, and after the final runtime wait in LLVM IR.

// OMP-LABEL: func.func @main
// OMP: call @carts_e2e_timer_get_time_ns()
// OMP: arts.edt <parallel>
// OMP: arts.barrier
// OMP: call @carts_e2e_timer_get_time_ns()

// EPOCH-LABEL: func.func @main
// EPOCH: call @carts_e2e_timer_get_time_ns()
// EPOCH: arts.epoch
// EPOCH: call @carts_e2e_timer_get_time_ns()

// LLVM-LABEL: define i32 @mainBody
// LLVM: call i64 @carts_e2e_timer_get_time_ns()
// LLVM: call i1 @artsWaitOnHandle(i64
// LLVM: call i64 @carts_e2e_timer_get_time_ns()

module {}
