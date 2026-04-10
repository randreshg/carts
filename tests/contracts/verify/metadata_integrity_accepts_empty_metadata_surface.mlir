// RUN: %carts-compile %s --arts-config %S/../../examples/arts.cfg --verify-metadata-integrity >/dev/null

module {
  func.func @main(%arg0: i32) -> i32 {
    return %arg0 : i32
  }
}
