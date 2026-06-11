// RUN: not %carts-compile %s --pipeline create-dbs --start-from create-dbs --arts-config %inputs_dir/arts_1t.cfg 2>&1 | %FileCheck %s

// Read-only rematerialized globals can stay EDT-local. Mutable global state
// must not remain as raw memory inside an EDT.

// CHECK: writes or escapes an EDT-local memref.global

module {
  memref.global "private" @scratch : memref<1xf64> = dense<0.000000e+00>

  func.func @create_dbs_rejects_mutable_edt_global() {
    %route = arith.constant -1 : i32
    arts.edt <sync> <intranode> route(%route) {
    ^bb0:
      %c0 = arith.constant 0 : index
      %value = arith.constant 1.000000e+00 : f64
      %global = memref.get_global @scratch : memref<1xf64>
      memref.store %value, %global[%c0] : memref<1xf64>
      arts.yield
    }
    return
  }
}
