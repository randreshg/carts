// RUN: not %carts-compile %s --arts-config %arts_config --start-from codir-to-arts --pipeline codir-to-arts 2>&1 | %FileCheck %s

// CODIR-to-ARTS rejects residual SDE task ops at the boundary.

module {
  func.func @direct_sde_task_rejected() {
    sde.cu_task {
      sde.yield
    }
    func.return
  }
}

// CHECK: SDE operation reached CODIR-to-ARTS
// CHECK-SAME: materialize-sde-boundary-to-arts
