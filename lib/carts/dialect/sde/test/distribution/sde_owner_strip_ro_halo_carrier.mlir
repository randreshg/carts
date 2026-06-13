// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,verify-sde-mu-layout,raise-to-mu-access-window,verify-sde-mu-access-window,sde-redistribute)' 2>&1 | %FileCheck %s

// Rank-expanded owner-strip stencils need one read window per read-only
// neighbor input and a halo_like redist projected onto the expanded grid dim.

// CHECK-LABEL: func.func @owner_strip_three_ro_inputs
// CHECK-COUNT-3: sde.su_halo %{{.*}} : memref<16x8x8x4xf32> array_id({{[0-9]+}}) owner [0] block [1, 8, 8, 4] halo [1, 0, 0, 0]
// CHECK-COUNT-3: sde.mu_access_window read %{{.*}} : memref<16x8x8x4xf32> array_id({{[0-9]+}})

func.func @owner_strip_three_ro_inputs() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c4 = arith.constant 4 : index
  %c8 = arith.constant 8 : index
  %c64 = arith.constant 64 : index
  %zero = arith.constant 0.0 : f32
  %B = sde.mu_alloc : memref<8x8x64xf32>
  %C = sde.mu_alloc : memref<8x8x64xf32>
  %D = sde.mu_alloc : memref<8x8x64xf32>
  %A = sde.mu_alloc : memref<8x8x64xf32>

  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
  ^bb0(%k: index):
    sde.array_layout_root write %B : memref<8x8x64xf32> array_id(0)
    sde.array_layout_root write %C : memref<8x8x64xf32> array_id(1)
    sde.array_layout_root write %D : memref<8x8x64xf32> array_id(2)
    sde.cu_region <single> {
      scf.for %i = %c0 to %c8 step %c1 {
        scf.for %j = %c0 to %c8 step %c1 {
          memref.store %zero, %B[%i, %j, %k] : memref<8x8x64xf32>
          memref.store %zero, %C[%i, %j, %k] : memref<8x8x64xf32>
          memref.store %zero, %D[%i, %j, %k] : memref<8x8x64xf32>
        }
    }
      sde.yield
    }
  } {arrayLayout = [
      {arrayId = 0 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "write", commVolumeBytes = 0 : i64},
      {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "write", commVolumeBytes = 0 : i64},
      {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "write", commVolumeBytes = 0 : i64}
    ]}

  sde.su_distribute <owner_compute> {
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
  ^bb0(%k: index):
    sde.array_layout_root read %B : memref<8x8x64xf32> array_id(0)
    sde.array_layout_root read %C : memref<8x8x64xf32> array_id(1)
    sde.array_layout_root read %D : memref<8x8x64xf32> array_id(2)
    sde.array_layout_root write %A : memref<8x8x64xf32> array_id(3)
    sde.cu_region <single> {
      %km1 = arith.subi %k, %c1 : index
      %kp1 = arith.addi %k, %c1 : index
      scf.for %i = %c0 to %c8 step %c1 {
        scf.for %j = %c0 to %c8 step %c1 {
          %b = memref.load %B[%i, %j, %km1] : memref<8x8x64xf32>
          %c = memref.load %C[%i, %j, %kp1] : memref<8x8x64xf32>
          %d = memref.load %D[%i, %j, %kp1] : memref<8x8x64xf32>
          %bc = arith.addf %b, %c : f32
          %sum = arith.addf %bc, %d : f32
          memref.store %sum, %A[%i, %j, %k] : memref<8x8x64xf32>
        }
    }
      sde.yield
    }
  } {arrayLayout = [
      {arrayId = 0 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "read", commVolumeBytes = 64 : i64},
      {arrayId = 1 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "read", commVolumeBytes = 64 : i64},
      {arrayId = 2 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "read", commVolumeBytes = 64 : i64},
      {arrayId = 3 : i64, kind = "block_parallel", ownerDims = [2], blockShape = [8, 8, 4], muBlockCount = 16 : i64, role = "write", commVolumeBytes = 0 : i64}
    ], accessMinOffsets = [-1], accessMaxOffsets = [1], inPlaceSafe}
  }
  memref.dealloc %B : memref<8x8x64xf32>
  memref.dealloc %C : memref<8x8x64xf32>
  memref.dealloc %D : memref<8x8x64xf32>
  memref.dealloc %A : memref<8x8x64xf32>
  return
}

// CHECK-LABEL: func.func @in_place_gauss_seidel_gets_readwrite_window
// CHECK: sde.mu_access_window readwrite %{{.*}} : memref<16x8x8x4xf32> array_id(4)

func.func @in_place_gauss_seidel_gets_readwrite_window() {
  %c0 = arith.constant 0 : index
  %c1 = arith.constant 1 : index
  %c8 = arith.constant 8 : index
  %c64 = arith.constant 64 : index
  %A = sde.mu_alloc : memref<8x8x64xf32>
  sde.su_iterate (%c0) to (%c64) step (%c1) classification(<stencil>) {
  ^bb0(%k: index):
    sde.array_layout_root write %A : memref<8x8x64xf32> array_id(4)
    sde.cu_region <single> {
      %kp1 = arith.addi %k, %c1 : index
      scf.for %i = %c0 to %c8 step %c1 {
        scf.for %j = %c0 to %c8 step %c1 {
          %v = memref.load %A[%i, %j, %kp1] : memref<8x8x64xf32>
          memref.store %v, %A[%i, %j, %k] : memref<8x8x64xf32>
        }
    }
      sde.yield
    }
  } {arrayLayout = [{arrayId = 4 : i64, blockShape = [8, 8, 4], commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 16 : i64, ownerDims = [2], role = "write"}], accessMinOffsets = [-1], accessMaxOffsets = [1]}
  return
}
