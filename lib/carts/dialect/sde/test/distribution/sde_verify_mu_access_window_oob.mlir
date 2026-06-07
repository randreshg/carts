// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Op verifier (structural bound): a window whose block_hi exceeds the MU
// grid dim is rejected by the op's own C++ verifier (SdeMuAccessWindowOp::verify)
// during op verification, before any pass runs. Here block_hi [99] exceeds the
// grid dim 4 of memref<4x256xf32>.

// CHECK: error: {{.*}}exceeds MU grid dim

func.func @window_out_of_bounds() {
  %A = sde.mu_alloc : memref<4x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window write %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [99] valid [256]
    sde.yield
  }
  return
}
