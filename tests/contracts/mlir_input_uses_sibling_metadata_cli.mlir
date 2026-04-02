// RUN: sh -c 'rm -rf %t.dir && mkdir -p %t.dir && cd %t.dir && %carts compile %S/inputs/mlir_metadata_autodiscovery/input.mlir --start-from initial-cleanup --pipeline initial-cleanup --arts-config %S/../examples/arts.cfg -o out.mlir >/dev/null && cat out.mlir' | %FileCheck %s

// CHECK: arts.id = 9 : i64
// CHECK-SAME: arts.loop = #arts.loop_metadata<
// CHECK-SAME: tripCount = 4 : i64
// CHECK-SAME: locationKey = "metadata_fixture.c:7:3"
