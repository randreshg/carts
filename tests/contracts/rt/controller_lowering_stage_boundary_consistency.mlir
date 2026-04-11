// RUN: %carts-compile %S/../inputs/uniform_block.mlir --O3 --arts-config %S/../../examples/arts.cfg --pipeline post-db-refinement | %FileCheck %s --check-prefix=DB-POLICY
// RUN: %carts-compile %S/../inputs/uniform_block.mlir --O3 --arts-config %S/../../examples/arts.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=EDT-BND
// RUN: %carts-compile %S/../inputs/uniform_block.mlir --O3 --arts-config %S/../../examples/arts.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=EPOCH-BND
// RUN: %carts-compile %S/../inputs/uniform_block.mlir --O3 --arts-config %S/../inputs/arts_multinode.cfg --pipeline edt-distribution | %FileCheck %s --check-prefix=DIST-EDT
// RUN: %carts-compile %S/../inputs/uniform_block.mlir --O3 --arts-config %S/../inputs/arts_multinode.cfg --pipeline pre-lowering | %FileCheck %s --check-prefix=DIST-PRE

// DB-POLICY-LABEL: func.func @main
// DB-POLICY: arts.db_alloc[<inout>, <heap>, <write>, <coarse>, <uniform>]
// DB-POLICY: arts.lowering_contract(
// DB-POLICY-SAME: block_shape[
// DB-POLICY-SAME: contract(<ownerDims = [0], postDbRefined = true>)
// DB-POLICY: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[
// DB-POLICY-SAME: ), offsets[

// EDT-BND-LABEL: func.func @main
// EDT-BND: arts.epoch attributes {{.*}}distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32
// EDT-BND: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[
// EDT-BND-SAME: ), offsets[
// EDT-BND: arts.edt <task> <intranode> route(%{{.*}}) {{.*}}attributes {{.*}}distribution_kind = #arts.distribution_kind<block>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32

// EPOCH-BND-LABEL: func.func @main
// EPOCH-BND: %[[EPOCH:.+]] = arts_rt.create_epoch : i64
// EPOCH-BND: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[
// EPOCH-BND-SAME: ), offsets[
// EPOCH-BND: %[[TASK:.+]] = arts_rt.edt_create({{.*}}) depCount(%{{.*}}) route(%{{.*}}) epoch(%[[EPOCH]] : i64)
// EPOCH-BND: arts_rt.rec_dep %[[TASK]]
// EPOCH-BND: arts_rt.wait_on_epoch %[[EPOCH]] : i64
// EPOCH-BND-NOT: arts.epoch

// DIST-EDT-LABEL: func.func @main
// DIST-EDT: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[
// DIST-EDT-SAME: ), offsets[
// DIST-EDT: %[[NODE:.+]] = arith.divui %arg0, %{{.*}} : index
// DIST-EDT: %[[ROUTE:.+]] = arith.index_cast %[[NODE]] : index to i32
// DIST-EDT: arts.edt <task> <internode> route(%[[ROUTE]]) {{.*}}attributes {{.*}}distribution_kind = #arts.distribution_kind<two_level>, distribution_pattern = #arts.distribution_pattern<uniform>, distribution_version = 1 : i32

// DIST-PRE-LABEL: func.func @main
// DIST-PRE: %[[EPOCH:.+]] = arts_rt.create_epoch : i64
// DIST-PRE: arts.db_acquire[<inout>] {{.*}}partitioning(<block>, offsets[
// DIST-PRE-SAME: ), offsets[
// DIST-PRE: %[[ROUTE:.+]] = arith.index_cast %{{.*}} : index to i32
// DIST-PRE: %[[TASK:.+]] = arts_rt.edt_create({{.*}}) depCount(%{{.*}}) route(%[[ROUTE]]) epoch(%[[EPOCH]] : i64)
// DIST-PRE: arts_rt.wait_on_epoch %[[EPOCH]] : i64
