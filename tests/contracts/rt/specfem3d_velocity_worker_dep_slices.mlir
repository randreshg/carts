// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../../external/carts-benchmarks/specfem3d/velocity/velocity_update.c --pipeline pre-lowering --arts-config %S/../inputs/arts_64t.cfg -O3 -- -DNX=288 -DNY=288 -DNZ=384 -DNREPS=10 -I%S/../../../external/carts-benchmarks/specfem3d/common >/dev/null && cat %t.compile/velocity_update.pre-lowering.mlir' | %FileCheck %s

// Velocity worker dependencies currently stay conservative through
// pre-lowering: the read acquires remain whole-DB coarse views carrying the
// owner-dimension contract, and rec_dep lowers without byte-slice transport.
// CHECK: arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>), offsets[%c0], sizes[%c1]
// CHECK: arts.lowering_contract({{.*}}contract(<ownerDims = [2], postDbRefined = true
// CHECK: arts_rt.rec_dep
// CHECK-NOT: byte_offsets(
// CHECK-NOT: byte_sizes(

module {
}
