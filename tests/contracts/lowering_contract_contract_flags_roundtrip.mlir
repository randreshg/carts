// RUN: %carts-compile %s --arts-config %S/../examples/arts.cfg --start-from post-db-refinement --pipeline post-db-refinement | %FileCheck %s

// CHECK: arts.lowering_contract({{.*}}) dep_pattern(<stencil>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[{{.*}}] min_offsets[{{.*}}] max_offsets[{{.*}}] write_footprint[{{.*}}] {contract_kind = 1 : i64, narrowable_dep, owner_dims = array<i64: 0>, spatial_dims = array<i64: 0>, supported_block_halo}

module {
  func.func @main() {
    %c0 = arith.constant 0 : index
    %c1 = arith.constant 1 : index
    %c4 = arith.constant 4 : index
    %cm1 = arith.constant -1 : index
    %alloc = memref.alloc() : memref<16xf32>
    arts.lowering_contract(%alloc : memref<16xf32>) dep_pattern(<stencil>) distribution_kind(<block>) distribution_pattern(<stencil>) block_shape[%c4] min_offsets[%cm1] max_offsets[%c1] write_footprint[%c0] {contract_kind = 1 : i64, narrowable_dep, owner_dims = array<i64: 0>, spatial_dims = array<i64: 0>, supported_block_halo}
    return
  }
}
