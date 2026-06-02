// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_64t.cfg --start-from sde-planning --pipeline sde-to-codir --mlir-print-ir-after-all 2>&1 | %FileCheck %s

// Module-scoped affine-driven per-array BLOCK layout assignment. The
// `sde-layout-assignment` pass stamps one element-space BLOCK layout per array
// root on BOTH the writer and every reader scheduling unit, plus a per-edge
// geometric layouts-disagree marker and an abstract comm-volume estimate. It is
// pattern-free: SDE names data layouts and abstract edge pressure, not
// collectives.
//
// This is a chained matmul (E = A*B, F = C*D, G = E*F). The third matmul
// contracts the sibling-computed intermediate F on its row axis. Expected layouts:
//   - matmul outputs (E, F, G): block_parallel owner-tiled on [0, 1].
//   - matmul inputs (A, C, E): block_parallel row-block on [0].
//   - F as consumed by G on its contraction axis: block_contraction owner [0].
//   - the G scheduling unit carries layoutsDisagree only for F's contraction
//     read plus an abstract commVolumeBytes. NO collective name appears.

// CHECK-LABEL: // -----// IR Dump After LayoutAssignment (sde-layout-assignment) //----- //
// CHECK: func.func @three_mm

// First matmul: E = A*B. E (arrayId 0) is the writer's output, block_parallel
// owner [0]; A row-block owner [0]; B col-block owner [1]. All aligned, so the
// unit carries no layoutsDisagree and an abstract commVolumeBytes of 0. No
// collective name appears anywhere.
// CHECK: arrayLayout = [{arrayId = 0 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}, {arrayId = 1 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 2 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [1], role = "read"}], commVolumeBytes = 0 : i64

// Second matmul: F = C*D. F (arrayId 3) is consumed on its contraction axis by
// G and is a sibling-distributed intermediate, so even on its own writer its
// chosen home layout is block_contraction owner [0].
// CHECK: arrayLayout = [{arrayId = 3 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "write"}

// Third matmul: G = E*F. F appears again as block_contraction; the G unit marks
// layoutsDisagree for only the contraction read of F, not for aligned E/G.
// CHECK: arrayLayout = [{arrayId = 0 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 3 : i64, {{.*}}commVolumeBytes = 2097152 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 6 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}], commVolumeBytes = 2097152 : i64
// CHECK-SAME: layoutsDisagree = [3]

// Boundary proof: CODIR receives the same neutral layout graph facts under CODIR
// attr names. The per-array layout graph (block kinds, owner dims, per-edge
// commVolumeBytes) crosses the boundary verbatim as `array_layout`; CODIR does
// not carry a duplicate aggregate comm-volume attr. The dependency-to-array join
// crosses as `dep_array_ids`, and the disagreeing edge as `layouts_disagree`.
// Concrete collective selection remains a CODIR storage-planning decision.
// CHECK-LABEL: // -----// IR Dump After ConvertSdeToCodir
// CHECK: func.func @three_mm
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [
// CHECK: codir.codelet
// CHECK: codir.codelet
// CHECK-SAME: array_layout = [{arrayId = 0 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 3 : i64, {{.*}}commVolumeBytes = 2097152 : i64, kind = "block_contraction", muBlockCount = 2 : i64, ownerDims = [0], role = "read"}, {arrayId = 6 : i64, {{.*}}commVolumeBytes = 0 : i64, kind = "block_parallel", muBlockCount = 4 : i64, ownerDims = [0, 1], role = "write"}]
// CHECK-NOT: comm_volume_bytes
// CHECK-SAME: dep_array_ids = [0, 3, 6]
// CHECK-SAME: layouts_disagree = [3]

module attributes {
  dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">>,
  llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128",
  llvm.target_triple = "aarch64-unknown-linux-gnu"
} {
  func.func @three_mm(%A: memref<1024x1024xf32>, %B: memref<1024x1024xf32>,
                      %C: memref<1024x1024xf32>, %D: memref<1024x1024xf32>,
                      %E: memref<1024x1024xf32>, %F: memref<1024x1024xf32>,
                      %G: memref<1024x1024xf32>) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c1024 = arith.constant 1024 : index
    sde.cu_region <parallel> {
      // E = A * B
      sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        scf.for %k = %c0 to %c1024 step %c1 {
          %a = memref.load %A[%i, %k] : memref<1024x1024xf32>
          %b = memref.load %B[%k, %j] : memref<1024x1024xf32>
          %e = memref.load %E[%i, %j] : memref<1024x1024xf32>
          %p = arith.mulf %a, %b : f32
          %s = arith.addf %e, %p : f32
          memref.store %s, %E[%i, %j] : memref<1024x1024xf32>
        }
        sde.yield
      }
      // F = C * D
      sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        scf.for %k = %c0 to %c1024 step %c1 {
          %c = memref.load %C[%i, %k] : memref<1024x1024xf32>
          %d = memref.load %D[%k, %j] : memref<1024x1024xf32>
          %f = memref.load %F[%i, %j] : memref<1024x1024xf32>
          %p = arith.mulf %c, %d : f32
          %s = arith.addf %f, %p : f32
          memref.store %s, %F[%i, %j] : memref<1024x1024xf32>
        }
        sde.yield
      }
      // G = E * F  (contracts F on F's row dim k)
      sde.su_iterate (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
      ^bb0(%i: index, %j: index):
        scf.for %k = %c0 to %c1024 step %c1 {
          %e = memref.load %E[%i, %k] : memref<1024x1024xf32>
          %f = memref.load %F[%k, %j] : memref<1024x1024xf32>
          %g = memref.load %G[%i, %j] : memref<1024x1024xf32>
          %p = arith.mulf %e, %f : f32
          %s = arith.addf %g, %p : f32
          memref.store %s, %G[%i, %j] : memref<1024x1024xf32>
        }
        sde.yield
      }
      sde.yield
    }
    return
  }
}
