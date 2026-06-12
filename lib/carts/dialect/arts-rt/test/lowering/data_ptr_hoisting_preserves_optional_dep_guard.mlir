// RUN: %carts-compile %s --pass-pipeline='builtin.module(arts-data-ptr-hoisting)' \
// RUN:   | %FileCheck %s

// ARTS-RT may hoist unguarded invariant dep-backed data loads for performance,
// but it must not move a load out of the control region guarding a nullable
// boundary dep slot.

// CHECK-LABEL: func.func @__arts_edt_guarded_optional_dep
// CHECK: scf.for
// CHECK-NOT: memref.load
// CHECK: scf.if
// CHECK: memref.load
// CHECK: memref.store

// CHECK-LABEL: func.func @__arts_edt_unguarded_dep
// CHECK: %[[LOAD:.*]] = memref.load
// CHECK: scf.for
// CHECK-NOT: memref.load
// CHECK: memref.store %[[LOAD]]

module {
  func.func @__arts_edt_guarded_optional_dep(%depv: !llvm.ptr,
                                             %out: memref<?xf32>,
                                             %use_neighbor: i1) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %dep_guid, %dep_ptr = arts_rt.dep_db_acquire(%depv) offset[%c0 : index]
        bounds_valid(%use_neighbor) : !llvm.ptr -> memref<?xi64>, memref<?xf32>
    scf.for %i = %c0 to %c4 step %c1 {
      scf.if %use_neighbor {
        %value = memref.load %dep_ptr[%c0] : memref<?xf32>
        memref.store %value, %out[%i] : memref<?xf32>
      }
    }
    return
  }

  func.func @__arts_edt_unguarded_dep(%depv: !llvm.ptr,
                                      %out: memref<?xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %dep_guid, %dep_ptr = arts_rt.dep_db_acquire(%depv) offset[%c0 : index]
        : !llvm.ptr -> memref<?xi64>, memref<?xf32>
    scf.for %i = %c0 to %c4 step %c1 {
      %value = memref.load %dep_ptr[%c0] : memref<?xf32>
      memref.store %value, %out[%i] : memref<?xf32>
    }
    return
  }
}
