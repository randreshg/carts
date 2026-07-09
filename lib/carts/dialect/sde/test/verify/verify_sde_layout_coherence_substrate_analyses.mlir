// RUN: %carts-compile %s --pass-pipeline='builtin.module(verify-sde-layout-coherence)'

module {
  func.func @touch_substrate_analyses() {
    return
  }
}
