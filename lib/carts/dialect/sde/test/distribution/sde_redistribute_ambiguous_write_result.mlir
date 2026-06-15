// RUN: not %carts-compile %s --pass-pipeline='builtin.module(sde-redistribute)' 2>&1 | %FileCheck %s

// CHECK: partial-reduction reduce_scatter has multiple committed write-result targets

func.func @ambiguous_write_result(%Tmp: memref<256xf32>,
                                  %Y: memref<64xf32>,
                                  %Z: memref<64xf32>) {
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
    sde.array_layout_root write %Z : memref<64xf32> array_id(3)
    sde.cu_region <single> {
      memref.store %zero, %Y[%j] : memref<64xf32>
      memref.store %zero, %Z[%j] : memref<64xf32>
      scf.for %i = %c0 to %c256 step %c1 {
        %tmp = memref.load %Tmp[%i] : memref<256xf32>
        %old = memref.load %Y[%j] : memref<64xf32>
        %next = arith.addf %old, %tmp : f32
        memref.store %next, %Y[%j] : memref<64xf32>
      }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 0 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [128], muBlockCount = 2 : i64, role = "read"}, {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}, {arrayId = 3 : i64, kind = "block_parallel", ownerDims = [0], blockShape = [64], muBlockCount = 1 : i64, role = "write"}]}
  }
  return
}
