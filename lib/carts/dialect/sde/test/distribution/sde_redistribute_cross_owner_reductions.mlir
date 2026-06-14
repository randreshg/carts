// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// Cross-owner reductions are backed either by the consumer's actual
// reduction-indexed access to the source owner dim or by an explicit
// partial-reduction write result.

// CHECK-LABEL: func.func @reduction_indexed_read_backs_reduce_scatter
// CHECK: sde.su_reduce_scatter %{{.*}} : memref<256x64xf32> array_id(20) owner [0] block [128, 64] reduce 0 kind <add>
func.func @reduction_indexed_read_backs_reduce_scatter(%T: memref<256x64xf32>,
                                                       %G: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c256 = arith.constant 256 : index
  %zero = arith.constant 0.0 : f32
  sde.su_iterate (%c0, %c0) to (%c256, %c64) step (%c1, %c1) {
  ^bb0(%i: index, %j: index):
    sde.array_layout_root write %T : memref<256x64xf32> array_id(20)
    sde.cu_region <single> {
      memref.store %zero, %T[%i, %j] : memref<256x64xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 20 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 64], muBlockCount = 2 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0) to (%c64) step (%c1) {
  ^bb0(%j: index):
    sde.array_layout_root read %T : memref<256x64xf32> array_id(20)
    sde.array_layout_root write %G : memref<64xf32> array_id(21)
    sde.cu_region <single> {
      memref.store %zero, %G[%j] : memref<64xf32>
      scf.for %i = %c0 to %c256 step %c1 {
        %t = memref.load %T[%i, %j] : memref<256x64xf32>
        %old = memref.load %G[%j] : memref<64xf32>
        %next = arith.addf %old, %t : f32
        memref.store %next, %G[%j] : memref<64xf32>
      }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 20 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128, 64], muBlockCount = 2 : i64, role = "read"}, {arrayId = 21 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}]}
  }
  return
}

// CHECK-LABEL: func.func @unstamped_owner_reduction_targets_write_result
// CHECK: sde.su_reduce_scatter %{{.*}} : memref<64xf32> array_id(2) owner [0] block [64] reduce 0 kind <add>
// CHECK-NOT: sde.su_reduce_scatter %{{.*}} : memref<256xf32> array_id(0)
func.func @unstamped_owner_reduction_targets_write_result(%Tmp: memref<256xf32>,
                                                          %Y: memref<64xf32>) {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c64 = arith.constant 64 : index
  %c256 = arith.constant 256 : index
  %zero = arith.constant 0.0 : f32
  sde.su_iterate (%c0) to (%c256) step (%c1) {
  ^bb0(%i: index):
    sde.array_layout_root write %Tmp : memref<256xf32> array_id(0)
    sde.cu_region <single> {
      memref.store %zero, %Tmp[%i] : memref<256xf32>
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128], muBlockCount = 2 : i64, role = "write"}]}
  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<elementwise_pipeline>) {
  ^bb0(%j: index):
    sde.array_layout_root read %Tmp : memref<256xf32> array_id(0)
    sde.array_layout_root write %Y : memref<64xf32> array_id(2)
    sde.cu_region <single> {
      memref.store %zero, %Y[%j] : memref<64xf32>
      scf.for %i = %c0 to %c256 step %c1 {
        %tmp = memref.load %Tmp[%i] : memref<256xf32>
        %old = memref.load %Y[%j] : memref<64xf32>
        %next = arith.addf %old, %tmp : f32
        memref.store %next, %Y[%j] : memref<64xf32>
      }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128], muBlockCount = 2 : i64, role = "read"}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}]}
  }
  return
}
