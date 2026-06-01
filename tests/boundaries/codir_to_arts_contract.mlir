// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-codir,storage-planning,verify-codir,materialize-sde-boundary-to-arts,convert-codir-to-arts,verify-arts-objects-only)' \
// RUN:   --arts-config %inputs_dir/arts_8t.cfg \
// RUN:   | %FileCheck %s --implicit-check-not=codir.
// RUN: not %carts-compile %s --pass-pipeline='builtin.module(materialize-sde-boundary-to-arts,convert-codir-to-arts)' \
// RUN:   --arts-config %inputs_dir/arts_8t.cfg 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=UNPLANNED

// Per-dialect handoff contract: after `convert-codir-to-arts` runs and
// `verify-arts-objects-only` accepts the result, no `codir.*` op may
// survive. Storage-planning runs first to install codelet plan metadata
// that the conversion consumes. The implicit check guards against
// codelets, yields, or codir attribute fragments leaking into the ARTS
// dialect.

// CHECK-LABEL: func.func @codir_to_arts_contract
// CHECK: arts.db_alloc
// CHECK: arts.db_acquire
// CHECK: arts.edt

// UNPLANNED: requires one dep_owner_dims entry per dependency before CODIR-to-ARTS materialization

module {
  func.func @codir_to_arts_contract() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %A = memref.alloc() : memref<8xf32>
    scf.for %i = %c0 to %c8 step %c1 {
      codir.codelet deps(%A : memref<8xf32>) params(%i : index)
          attributes {dep_modes = [#codir.access_mode<readwrite>],
                      dep_storage_views = [#codir.storage_view<host_whole>]} {
      ^bb0(%arg0: memref<8xf32>, %j: index):
        %v = memref.load %arg0[%j] : memref<8xf32>
        memref.store %v, %arg0[%j] : memref<8xf32>
        codir.yield
      }
    }
    return
  }
}
