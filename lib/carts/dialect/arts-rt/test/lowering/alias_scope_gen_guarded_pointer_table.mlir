// RUN: %carts-compile %s --pass-pipeline="builtin.module(arts-alias-scope-gen)" | %FileCheck %s

// Dep base pointers reach the EDT compute body through a DataPtrHoisting
// null-guard `select(valid, fallback, depField)` and a second-level row-pointer
// load (pointer-table indirection). The per-array alias scopes must still be
// attached to the data accesses derived from those guarded/loaded pointers so
// the backend can prove the arrays do not alias and vectorize the inner loop.

module {
  // CHECK-LABEL: llvm.func @__arts_edt_0
  // CHECK: llvm.intr.experimental.noalias.scope.decl
  // CHECK: llvm.intr.experimental.noalias.scope.decl
  llvm.func @__arts_edt_0(%arg0: i32, %arg1: !llvm.ptr, %arg2: i32, %arg3: !llvm.ptr) {
    %true = llvm.mlir.constant(true) : i1
    %c1 = llvm.mlir.constant(1 : i64) : i64
    %c0 = llvm.mlir.constant(0 : i64) : i64
    %v = llvm.mlir.constant(1.000000e+00 : f32) : f32

    // dep slot 0: single-level field GEP `depv[0, 1]` (constant entry index 0).
    %f0 = llvm.getelementptr %arg3[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %p0 = llvm.load %f0 : !llvm.ptr -> !llvm.ptr

    // dep slot 1: two-level `depv[1]` entry then field `[0, 1]`.
    %e1 = llvm.getelementptr %arg3[1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %f1 = llvm.getelementptr %e1[0, 1] : (!llvm.ptr) -> !llvm.ptr, !llvm.struct<(i64, ptr, i32, i32, i64, i64)>
    %p1 = llvm.load %f1 : !llvm.ptr -> !llvm.ptr

    // Guarded pointer-table for slot 0, then second-level row-pointer load.
    %a0 = llvm.alloca %c1 x !llvm.ptr : (i64) -> !llvm.ptr
    %g0 = llvm.select %true, %a0, %f0 : i1, !llvm.ptr
    %t0 = llvm.getelementptr %g0[%c0] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %row0 = llvm.load %t0 : !llvm.ptr -> !llvm.ptr

    // Guarded pointer-table for slot 1.
    %a1 = llvm.alloca %c1 x !llvm.ptr : (i64) -> !llvm.ptr
    %g1 = llvm.select %true, %a1, %f1 : i1, !llvm.ptr
    %t1 = llvm.getelementptr %g1[%c0] : (!llvm.ptr, i64) -> !llvm.ptr, !llvm.ptr
    %row1 = llvm.load %t1 : !llvm.ptr -> !llvm.ptr

    // Data accesses through the guarded/loaded row pointers must be scoped.
    // CHECK: llvm.load %{{.*}} {alias_scopes = [{{.*}}], noalias_scopes = [{{.*}}]}
    %d0 = llvm.getelementptr %row0[%c0] : (!llvm.ptr, i64) -> !llvm.ptr, f32
    %ld = llvm.load %d0 : !llvm.ptr -> f32
    // CHECK: llvm.store %{{.*}} {alias_scopes = [{{.*}}], noalias_scopes = [{{.*}}]}
    %d1 = llvm.getelementptr %row1[%c0] : (!llvm.ptr, i64) -> !llvm.ptr, f32
    llvm.store %ld, %d1 : f32, !llvm.ptr
    llvm.return
  }
}
