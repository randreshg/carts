// RUN: not %carts-compile %s --pass-pipeline='builtin.module(verify-sde-mu-access-window)' 2>&1 | %FileCheck %s

// Op verifier: a window must describe a read XOR write access. A `readwrite`
// (in-place) window is rejected by the op's own C++ verifier at op-verification
// time (before any pass runs). The `readwrite` enum value still exists for other
// ops (sde.mu_token / sde.su_iterate); only sde.mu_access_window forbids it.

// CHECK: error: {{.*}}mode must be read or write

func.func @readwrite_window() {
  %A = sde.mu_alloc : memref<4x256xf32>
  sde.cu_region <parallel> {
    sde.mu_access_window readwrite %A : memref<4x256xf32> owner_dims(1) block_lo [0] block_hi [4] valid [256]
    sde.yield
  }
  return
}
