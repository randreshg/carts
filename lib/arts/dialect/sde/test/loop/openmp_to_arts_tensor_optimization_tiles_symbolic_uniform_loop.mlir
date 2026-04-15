// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=TENSOR
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=OPT
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that executable tensor tiling in SDE also handles symbolic trip
// counts: RaiseToTensor materializes the tensor-backed carrier, and
// Tiling synthesizes a symbolic tile size and rewrites the
// surrounding sde.su_iterate so the tiled shape survives into arts.for.

// TENSOR-LABEL: // -----// IR Dump After RaiseToLinalg (raise-to-linalg) //----- //
// TENSOR: arts_sde.su_iterate (%c0) to (%arg2) step (%c1) classification(<elementwise>) {
// TENSOR: arts_sde.cu_region <parallel>
// TENSOR: %[[IN:.+]] = arts_sde.mu_memref_to_tensor %arg0 : memref<?xf64> -> tensor<?xf64>
// TENSOR: %[[OUT:.+]] = arts_sde.mu_memref_to_tensor %arg1 : memref<?xf64> -> tensor<?xf64>
// TENSOR: linalg.generic
// TENSOR-SAME: ins(%[[IN]] : tensor<?xf64>)
// TENSOR-SAME: outs(%[[OUT]] : tensor<?xf64>)

// OPT-LABEL: // -----// IR Dump After Tiling (tiling) //----- //
// OPT: arts_sde.cu_region <parallel> {
// OPT: = arith.subi %arg2, %c0 : index
// OPT: = arith.cmpi slt
// OPT: = arith.select
// OPT: = arith.ceildivsi
// OPT: %[[TSTEP:.+]] = arith.muli %c1, %{{.+}} : index
// OPT: arts_sde.su_iterate (%c0) to (%arg2) step (%[[TSTEP]]) classification(<elementwise>) {
// OPT: arts_sde.mu_memref_to_tensor %arg0 : memref<?xf64>
// OPT: arts_sde.mu_memref_to_tensor %arg1 : memref<?xf64>
// OPT: tensor.extract_slice
// OPT: tensor.extract_slice
// OPT: linalg.generic
// OPT: arith.mulf
// OPT: linalg.yield
// OPT: tensor.insert_slice
// OPT-NOT: bufferization.to_tensor

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: arts.for(%c0) to(%arg2) step(%{{.*}})
// ARTS: memref.subview
// ARTS: scf.for
// ARTS-NOT: bufferization.to_tensor
// ARTS-NOT: arts_sde.mu_memref_to_tensor
// ARTS-NOT: linalg.generic

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<?xf64>, %B: memref<?xf64>, %N: index) {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f64
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i) : index = (%c0) to (%N) step (%c1) {
          %v = memref.load %A[%i] : memref<?xf64>
          %r = arith.mulf %v, %cst : f64
          memref.store %r, %B[%i] : memref<?xf64>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
