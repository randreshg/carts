// RUN: %carts-compile %S/../examples/addition/addition.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s

// CHECK: arts.epoch attributes {distribution_kind = #arts.distribution_kind<two_level>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32} {
// CHECK: arts.edt <task> <internode>{{.*}}attributes {distribution_kind = #arts.distribution_kind<two_level>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32}

