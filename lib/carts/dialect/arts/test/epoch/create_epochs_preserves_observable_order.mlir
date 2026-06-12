// RUN: %carts-compile %s --arts-config %S/../../../../../../tests/inputs/arts_1t.cfg --start-from=epochs --pipeline=epochs | %FileCheck %s

// CHECK-LABEL: func.func @main
// CHECK: call @carts_kernel_timer_start
// CHECK: scf.for
// CHECK: arts.epoch
// CHECK: func.call @carts_kernel_timer_accum
// CHECK: call @carts_kernel_timer_print
// CHECK: call @carts_phase_timer_start
// CHECK: arts.epoch
// CHECK: func.call @checksum

module attributes {
  arts.runtime_total_nodes = 1 : i64,
  arts.runtime_total_workers = 4 : i64
} {
  func.func @main() {
    %route = arith.constant -1 : i32
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c2 = arith.constant 2 : index
    %timer = arith.constant 7 : i64

    func.call @carts_kernel_timer_start(%timer) : (i64) -> ()
    scf.for %rep = %c0 to %c2 step %c1 {
      arts.edt <task> <intranode> route(%route) {
        arts.yield
      }
      arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
      func.call @carts_kernel_timer_accum(%timer) : (i64) -> ()
    }

    func.call @carts_kernel_timer_print(%timer) : (i64) -> ()
    func.call @carts_phase_timer_start(%timer, %timer) : (i64, i64) -> ()

    scf.for %i = %c0 to %c2 step %c1 {
      arts.edt <task> <intranode> route(%route) {
        arts.yield
      }
    }
    arts.barrier {barrierReason = #arts.barrier_reason<required_memory>}
    func.call @checksum() : () -> ()
    return
  }

  func.func private @carts_kernel_timer_start(i64)
  func.func private @carts_kernel_timer_accum(i64)
  func.func private @carts_kernel_timer_print(i64)
  func.func private @carts_phase_timer_start(i64, i64)
  func.func private @checksum()
}
