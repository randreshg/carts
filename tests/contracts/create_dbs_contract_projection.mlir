// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at pattern-pipeline | %FileCheck %s --check-prefix=PATTERN
// RUN: %carts-compile %S/../examples/jacobi/for/jacobi-for.mlir --O3 --arts-config %S/../examples/arts.cfg --stop-at create-dbs | %FileCheck %s --check-prefix=DB

// Ensure pattern-stage semantic attrs survive the full pattern pipeline.
// PATTERN: depPattern = #arts.dep_pattern<jacobi_alternating_buffers>
// PATTERN: distribution_pattern = #arts.distribution_pattern<stencil>
// PATTERN: stencil_min_offsets = [-1]
// PATTERN: stencil_max_offsets = [1]
// PATTERN: stencil_owner_dims = [0]
// PATTERN: stencil_spatial_dims = [0, 1]
// PATTERN: stencil_supported_block_halo
// PATTERN: stencil_write_footprint = [0]

// Ensure CreateDbs projects semantic contracts onto DbAcquireOp attrs and the
// pointer-level lowering contract consumed by post-DB analysis/policy passes.
// DB: arts.db_acquire[<in>] {{.*}}depPattern = #arts.dep_pattern<jacobi_alternating_buffers>{{.*}}distribution_pattern = #arts.distribution_pattern<stencil>{{.*}}stencil_max_offsets = [1], stencil_min_offsets = [-1], stencil_owner_dims = [0], stencil_spatial_dims = [0, 1], stencil_supported_block_halo, stencil_write_footprint = [0]
// DB: arts.lowering_contract({{.*}}) dep_pattern(<jacobi_alternating_buffers>) distribution_pattern(<stencil>) min_offsets[{{.*}}] max_offsets[{{.*}}] write_footprint[{{.*}}] {owner_dims = array<i64: 0>, spatial_dims = array<i64: 0, 1>, supported_block_halo}
