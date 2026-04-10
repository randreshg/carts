// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --arts-epoch-finish-continuation --start-from=epochs --pipeline=pre-lowering | %FileCheck %s

// Multi-epoch CPS chains must propagate the iteration counter and outer epoch
// GUID through every outlined continuation function, not just the first child.
//
// CHECK-DAG: arts.cps_chain_id = "{{.*chain_0}}", arts.cps_iter_counter_param_idx = {{[0-9]+}} : i64, arts.cps_outer_epoch_param_idx = {{[0-9]+}} : i64
// CHECK-DAG: arts.cps_chain_id = "{{.*chain_1}}", arts.cps_iter_counter_param_idx = {{[0-9]+}} : i64, arts.cps_outer_epoch_param_idx = {{[0-9]+}} : i64
// CHECK-DAG: arts.cps_chain_id = "{{.*chain_2}}", arts.cps_iter_counter_param_idx = {{[0-9]+}} : i64, arts.cps_outer_epoch_param_idx = {{[0-9]+}} : i64
// CHECK-DAG: arts.cps_chain_id = "{{.*chain_3}}", arts.cps_iter_counter_param_idx = {{[0-9]+}} : i64, arts.cps_outer_epoch_param_idx = {{[0-9]+}} : i64

module attributes {dlti.dl_spec = #dlti.dl_spec<#dlti.dl_entry<f128, dense<128> : vector<2xi64>>, #dlti.dl_entry<f64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i8, dense<[8, 32]> : vector<2xi64>>, #dlti.dl_entry<i64, dense<64> : vector<2xi64>>, #dlti.dl_entry<i16, dense<[16, 32]> : vector<2xi64>>, #dlti.dl_entry<i128, dense<128> : vector<2xi64>>, #dlti.dl_entry<!llvm.ptr, dense<64> : vector<4xi64>>, #dlti.dl_entry<i32, dense<32> : vector<2xi64>>, #dlti.dl_entry<i1, dense<8> : vector<2xi64>>, #dlti.dl_entry<f16, dense<16> : vector<2xi64>>, #dlti.dl_entry<"dlti.endianness", "little">, #dlti.dl_entry<"dlti.stack_alignment", 128 : i64>>, llvm.data_layout = "e-m:e-i8:8:32-i16:16:32-i64:64-i128:128-n32:64-S128", llvm.target_triple = "aarch64-unknown-linux-gnu"} {

  func.func private @sink_a() -> ()
  func.func private @sink_b() -> ()
  func.func private @sink_c() -> ()
  func.func private @sink_d() -> ()

  func.func @test_cps_chain_deep_param_propagation() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c8 = arith.constant 8 : index
    %c0_i32 = arith.constant 0 : i32
    scf.for %t = %c0 to %c8 step %c1 {
      %e0 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_a() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
      %e1 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_b() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
      %e2 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_c() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
      %e3 = arts.epoch {
        arts.edt <task> <intranode> route(%c0_i32) {
        ^bb0:
          func.call @sink_d() : () -> ()
          arts.yield
        }
        arts.yield
      } : i64
    }
    return
  }
}
