// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline codir-to-arts \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=BASE
// RUN: %carts-compile %s --O3 --arts-config %inputs_dir/arts_multinode.cfg \
// RUN:   --start-from sde-planning --pipeline codir-to-arts \
// RUN:   --min-distributed-tile-bytes=4194304 \
// RUN:   --mlir-print-ir-after-all 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=COARSE

// 1024x1024 f64 matmul on the two-node test config (16 workers across the
// cluster). With no floor, DistributionPlanning factors workers across both
// owner dims and produces a fine-grained tile shape. With a 4 MiB floor,
// the writer shrinks the worker grid along the larger axis until each tile
// carries at least 4 MiB of output (524288 f64 elements). The floor is the
// only knob that changed between the two runs; everything else is held
// constant so the two physicalBlockShape lines come from the same writer.

// BASE: physicalBlockShape = [128, 256]
// COARSE: physicalBlockShape = [1024, 512]

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<1024x1024xf64>, %B: memref<1024x1024xf64>, %C: memref<1024x1024xf64>) {
    %c0 = arith.constant 0 : index
    %c1024 = arith.constant 1024 : index
    %c1 = arith.constant 1 : index
    omp.parallel {
      omp.wsloop {
        omp.loop_nest (%i, %j) : index = (%c0, %c0) to (%c1024, %c1024) step (%c1, %c1) {
          scf.for %k = %c0 to %c1024 step %c1 {
            %a = memref.load %A[%i, %k] : memref<1024x1024xf64>
            %b = memref.load %B[%k, %j] : memref<1024x1024xf64>
            %c = memref.load %C[%i, %j] : memref<1024x1024xf64>
            %prod = arith.mulf %a, %b : f64
            %sum = arith.addf %c, %prod : f64
            memref.store %sum, %C[%i, %j] : memref<1024x1024xf64>
          }
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
