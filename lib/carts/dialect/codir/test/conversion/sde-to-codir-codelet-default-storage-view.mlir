// RUN: %carts-compile %s --pass-pipeline='builtin.module(convert-sde-to-codir,verify-codir)' | %FileCheck %s --implicit-check-not=sde.cu_work

module {
  func.func @sde_whole_token_defaults_to_codir_host_whole(%mem: memref<8xf32>) {
    %token = sde.mu_token <read> %mem
      : memref<8xf32> -> !sde.token<memref<8xf32>>

    sde.cu_work (%token : !sde.token<memref<8xf32>>) {
    ^bb0(%view: memref<8xf32>):
      %c0 = arith.constant 0 : index
      %value = memref.load %view[%c0] : memref<8xf32>
      sde.yield
    }
    return
  }
}

// CHECK-LABEL: func.func @sde_whole_token_defaults_to_codir_host_whole
// CHECK: codir.codelet deps(%{{.*}} : memref<8xf32>)
// CHECK-SAME: dep_modes = [#codir.access_mode<read>]
// CHECK-SAME: dep_storage_views = [#codir.storage_view<host_whole>]
