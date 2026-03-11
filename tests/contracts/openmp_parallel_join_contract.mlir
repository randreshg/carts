// RUN: %carts-compile %S/../examples/parallel/parallel.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at openmp-to-arts | %FileCheck %s
// RUN: %carts-compile %S/../examples/parallel/parallel.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at epochs | %FileCheck %s --check-prefix=EPOCH
// RUN: %carts-compile %S/../examples/parallel/parallel.mlir --O3 --arts-config %S/../examples/arts.cfg --emit-llvm | %FileCheck %s --check-prefix=LLVM

// Contract: omp.parallel has an implicit join. After OpenMP-to-ARTS, later
// host work must remain ordered behind an explicit arts.barrier. After
// CreateEpochs, only the parallel EDT launch should move into the epoch; host
// timer state and continuation work must stay outside so later lowering keeps
// SSA dominance intact.

// CHECK: arts.edt <parallel>
// CHECK: arts.barrier
// CHECK: llvm.call @printf

// EPOCH: %[[TIMERBUF:.*]] = memref.get_global @_carts_timer_start
// EPOCH: %[[START:.*]] = call @omp_get_wtime()
// EPOCH: memref.store %[[START]], %[[TIMERBUF]]
// EPOCH: %[[WORKERS:.*]] = arts.runtime_query <total_workers>
// EPOCH: %[[EPOCH:.*]] = arts.epoch {
// EPOCH:   arts.edt <parallel>
// EPOCH:   llvm.call @printf
// EPOCH: }
// EPOCH: llvm.call @printf
// EPOCH: %[[END:.*]] = call @omp_get_wtime()
// EPOCH: %[[STARTLOAD:.*]] = memref.load %[[TIMERBUF]]

// LLVM: define i32 @mainBody() {
// LLVM: call i64 @artsEdtCreateWithEpochArtsId
// LLVM: call i1 @artsWaitOnHandle
// LLVM: call i32 (ptr, ...) @printf(ptr @str2)
// LLVM: load double, ptr @_carts_timer_start
// LLVM: ret i32

module {}
