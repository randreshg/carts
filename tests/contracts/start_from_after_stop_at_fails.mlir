// RUN: not %carts-compile %S/inputs/uniform_block.mlir --O3 --arts-config %S/../examples/arts.cfg --start-from pre-lowering --stop-at db-opt 2>&1 | %FileCheck %s

// CHECK: Invalid pipeline stage range: --start-from=pre-lowering is after --stop-at=db-opt
