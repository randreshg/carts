// RUN: %carts-compile %s --pipeline create-dbs --start-from create-dbs --arts-config %inputs_dir/arts_1t.cfg \
// RUN:   | %FileCheck %s --implicit-check-not="cannot trace memref operand" --implicit-check-not=arts.db_alloc

// CreateDbs only materializes raw shared EDT memory into DBs. EDT-private
// scratch and string literal views are local constants, not DB dependencies.

// CHECK-LABEL: func.func @create_dbs_skips_edt_private_string_memrefs
// CHECK: arts.edt <sync> <intranode>
// CHECK: memref.alloca
// CHECK: polygeist.pointer2memref

module {
  llvm.mlir.global internal constant @str0("ok\00") {addr_space = 0 : i32}

  func.func @create_dbs_skips_edt_private_string_memrefs() {
    %route = arith.constant -1 : i32
    arts.edt <sync> <intranode> route(%route) {
    ^bb0:
      %cst = arith.constant 1.000000e+00 : f64
      %scratch = memref.alloca() : memref<f64>
      memref.store %cst, %scratch[] : memref<f64>
      func.call @touch(%scratch) : (memref<f64>) -> ()
      %value = memref.load %scratch[] : memref<f64>
      %str = llvm.mlir.addressof @str0 : !llvm.ptr
      %msg = polygeist.pointer2memref %str : !llvm.ptr to memref<?xi8>
      %ignored = func.call @carts_test_pass_impl(%msg, %value) : (memref<?xi8>, f64) -> i32
      arts.yield
    }
    return
  }

  func.func private @carts_test_pass_impl(memref<?xi8>, f64) -> i32
  func.func private @touch(memref<f64>)
}
