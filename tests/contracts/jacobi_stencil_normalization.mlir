// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at concurrency-opt | %FileCheck %s

// CHECK: arts.epoch attributes {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK: arts.edt <task>
// CHECK: arts.epoch attributes {depPattern = #arts.dep_pattern<jacobi_alternating_buffers>, distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<stencil>
// CHECK-NOT: distribution_pattern = #arts.distribution_pattern<uniform>

module {
}
