// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db)' 2>&1 | %FileCheck %s --check-prefix=STORAGE --implicit-check-not=sde.mu_alloc
// RUN: %carts-compile %s --pass-pipeline='builtin.module(sde-rank-expand-mu,raise-to-mu-access-window,sde-storage-to-arts-db,sde-accesses-to-arts-deps,finalize-sde-to-arts,verify-arts-objects-only)' 2>&1 | %FileCheck %s --check-prefix=ARTS --implicit-check-not=sde.mu_alloc --implicit-check-not=sde.mu_access_window --implicit-check-not=arts.db_access_window

// STORAGE-LABEL: func.func @rank0_replicated_read
// STORAGE: arts.db_alloc
// STORAGE-SAME: <coarse>
// STORAGE: arts.db_access_window
// STORAGE-SAME: mode = #arts.mode<out>
// STORAGE-SAME: ownerDimCount = 0
// STORAGE-SAME: validExtents = [64]
// STORAGE: arts.db_access_window
// STORAGE-SAME: mode = #arts.mode<in>
// STORAGE-SAME: ownerDimCount = 0
// STORAGE-SAME: validExtents = [64]

// ARTS-LABEL: func.func @rank0_replicated_read
// ARTS: arts.db_alloc
// ARTS-SAME: <coarse>
// ARTS: arts.db_acquire[<out>]
// ARTS-SAME: partitioning(<coarse>)
// ARTS: arts.db_ref %{{.*}}[%{{.*}}]
// ARTS: memref.store %{{.*}}, %{{.*}}[%{{.*}}] : memref<?xf32>
// ARTS: arts.db_acquire[<in>]
// ARTS-SAME: partitioning(<coarse>)
// ARTS: arts.edt
// ARTS: arts.db_ref %{{.*}}[%{{.*}}]
// ARTS: memref.load %{{.*}}[%{{.*}}] : memref<?xf32>

module attributes {arts.runtime_total_nodes = 1 : i64, arts.runtime_total_workers = 4 : i64} {
  func.func @rank0_replicated_read() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c16 = arith.constant 16 : index
    %c64 = arith.constant 64 : index
    %cst = arith.constant 1.0 : f32
    %R = sde.mu_alloc : memref<64xf32>
    sde.cu_region <single> {
      scf.for %i = %c0 to %c64 step %c1 {
        memref.store %cst, %R[%i] : memref<64xf32>
      }
    }
    sde.su_iterate (%c0) to (%c64) step (%c16)
        classification(<elementwise>) {
    ^bb0(%base: index):
      sde.array_layout_root read %R : memref<64xf32> array_id(0)
      sde.cu_region <parallel> {
        %hi = arith.addi %base, %c16 : index
        %limit = arith.minui %hi, %c64 : index
        scf.for %i = %base to %limit step %c1 {
          %v = memref.load %R[%i] : memref<64xf32>
          func.call @sink(%v) : (f32) -> ()
        }
      }
    } {arrayLayout = [{arrayId = 0 : i64, kind = "replicated", ownerDims = [], blockShape = [64], budgetBlockShape = [64], muBlockCount = 1 : i64, role = "read"}]}
    return
  }

  func.func private @sink(f32)
}
