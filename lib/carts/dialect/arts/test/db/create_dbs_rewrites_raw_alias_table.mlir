// RUN: %carts-compile %s --pipeline create-dbs --start-from create-dbs --arts-config %inputs_dir/arts_1t.cfg | %FileCheck %s

// A raw external memref can be cast and stored into an EDT-local pointer table
// before later indexed reads. CreateDbs must acquire the external memref and
// rewrite the cast to the acquired DB view instead of leaving a host alloca
// pointer in the EDT-local table.

module {
  func.func @raw_alias_table() {
    %route = arith.constant -1 : i32
    %flag = arith.constant true
    %c0 = arith.constant 0 : index
    %buffer = memref.alloca() : memref<10xf64>
    arts.edt <sync> <intranode> route(%route) params(%flag : i1) {
    ^bb0(%flag_arg: i1):
      %table = memref.alloca() : memref<1xmemref<?xf64>>
      %cast = memref.cast %buffer : memref<10xf64> to memref<?xf64>
      memref.store %cast, %table[%c0] : memref<1xmemref<?xf64>>
      %selected = memref.load %table[%c0] : memref<1xmemref<?xf64>>
      scf.if %flag_arg {
        %value = memref.load %selected[%c0] : memref<?xf64>
        func.call @consume(%value) : (f64) -> ()
      }
      arts.yield
    }
    return
  }
  func.func private @consume(f64)
}

// CHECK-LABEL: func.func @raw_alias_table
// CHECK: %[[GUID:.*]], %[[DB:.*]] = arts.db_alloc[<in>
// CHECK: %{{.*}}, %[[ACQ:.*]] = arts.db_acquire[<in>] (%[[GUID]] : {{.*}}, %[[DB]]
// CHECK: arts.edt <sync> <intranode> route({{.*}}) (%[[ACQ]])
// CHECK-SAME: params(%true
// CHECK: ^bb0(%[[DEP:.*]]: memref{{.*}}, %{{.*}}: i1)
// CHECK: %[[VIEW:.*]] = arts.db_ref %[[DEP]]
// CHECK: memref.load %[[VIEW]]
// CHECK-NOT: memref.cast
