// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=SDE
// RUN: %carts-compile %s --O3 --arts-config %arts_config --pipeline openmp-to-arts --mlir-print-ir-after-all 2>&1 | %FileCheck %s --check-prefix=ARTS

// Verify that SDE fuses consecutive elementwise scheduling units before the
// ARTS boundary and lowers the fused family as elementwise_pipeline instead of
// relying on ARTS KernelTransforms to invent that pattern later.

// SDE-LABEL: // -----// IR Dump After StructuredSummaries (structured-summaries) //----- //
// SDE: func.func @main
// SDE: arts_sde.cu_region <parallel> {
// SDE-COUNT-2: arts_sde.su_iterate (%c0) to (%c128) step (%{{.+}}) nowait classification(<elementwise>) {

// SDE-LABEL: // -----// IR Dump After ElementwiseFusion (elementwise-fusion) //----- //
// SDE: func.func @main
// SDE: arts_sde.cu_region <parallel> {
// SDE-COUNT-1: arts_sde.su_iterate (%c0) to (%c128) step (%{{.+}}) nowait classification(<elementwise_pipeline>) {
// SDE: %[[A:.*]] = memref.load %arg0[%{{.*}}] : memref<128xf32>
// SDE: memref.store %{{.*}}, %arg1[%{{.*}}] : memref<128xf32>
// SDE: %[[B:.*]] = memref.load %arg1[%{{.*}}] : memref<128xf32>
// SDE: memref.store %{{.*}}, %arg2[%{{.*}}] : memref<128xf32>

// ARTS-LABEL: // -----// IR Dump After ConvertSdeToArts (convert-sde-to-arts) //----- //
// ARTS: func.func @main
// ARTS: arts.edt <parallel> <intranode> route(%{{.*}}) attributes {
// ARTS: arts.for(%c0) to(%c128) step(%{{.+}}) {
// ARTS: %[[A:.*]] = memref.load %arg0[%{{.*}}] : memref<128xf32>
// ARTS: memref.store %{{.*}}, %arg1[%{{.*}}] : memref<128xf32>
// ARTS: %[[B:.*]] = memref.load %arg1[%{{.*}}] : memref<128xf32>
// ARTS: memref.store %{{.*}}, %arg2[%{{.*}}] : memref<128xf32>
// ARTS: } {arts.pattern_revision = 1 : i64
// ARTS-SAME: {{.*}}depPattern = #arts.dep_pattern<elementwise_pipeline>{{.*}}distribution_kind = #arts.distribution_kind<block>{{.*}}distribution_pattern = #arts.distribution_pattern<uniform>{{.*}}distribution_version = 1 : i32
// ARTS-NOT: arts_sde.

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {
  func.func @main(%A: memref<128xf32>, %B: memref<128xf32>, %C: memref<128xf32>) {
    %c0 = arith.constant 0 : index
    %c128 = arith.constant 128 : index
    %c1 = arith.constant 1 : index
    %cst = arith.constant 2.0 : f32
    omp.parallel {
      omp.wsloop nowait {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %v = memref.load %A[%i] : memref<128xf32>
          %r = arith.addf %v, %cst : f32
          memref.store %r, %B[%i] : memref<128xf32>
          omp.yield
        }
      }
      omp.wsloop nowait {
        omp.loop_nest (%i) : index = (%c0) to (%c128) step (%c1) {
          %v = memref.load %B[%i] : memref<128xf32>
          %r = arith.mulf %v, %cst : f32
          memref.store %r, %C[%i] : memref<128xf32>
          omp.yield
        }
      }
      omp.terminator
    }
    return
  }
}
