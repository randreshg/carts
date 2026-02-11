// RUN: %carts-run %S/../examples/addition/addition.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTRA
// RUN: %carts-run %S/../examples/addition/addition.mlir --O3 --arts-config %S/../../docker/arts-docker-2node.cfg --stop-at edt-distribution | %FileCheck %s --check-prefix=INTER

// INTRA: distribution_kind = #arts.distribution_kind<block>
// INTRA: distribution_pattern = #arts.distribution_pattern<uniform>

// INTER: distribution_kind = #arts.distribution_kind<two_level>
// INTER: distribution_pattern = #arts.distribution_pattern<uniform>

