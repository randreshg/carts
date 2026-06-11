// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=post-db-refinement
// RUN: %FileCheck %s < %t.dir/jacobi-for.post-db-refinement.mlir

// Read-only halo deps acquire the committed neighboring DB window and pass that
// window origin into the EDT, so shifted stencil reads cannot collapse to the
// current block.

// CHECK: %[[C1:[A-Za-z0-9_]+]] = arith.constant 1 : index
// CHECK: arts.barrier {barrierReason = #arts.barrier_reason<unknown_required>}
// CHECK: %[[BLOCK_I:[A-Za-z0-9_]+]] = arith.divui %arg{{[0-9]+}}, %c32 : index
// CHECK: %[[BLOCK_J:[A-Za-z0-9_]+]] = arith.divui %arg{{[0-9]+}}, %c32 : index
// CHECK: arts.db_acquire[<in>] {{.*}} offsets[%[[BLOCK_I]], %[[BLOCK_J]]], sizes[%{{[A-Za-z0-9_]+}}, %{{[A-Za-z0-9_]+}}]
// CHECK: %[[CORE_REMAIN_I:[A-Za-z0-9_]+]] = arith.subi %c8, %[[BLOCK_I]] : index
// CHECK: %[[CORE_SIZE_I:[A-Za-z0-9_]+]] = arith.minui %[[CORE_REMAIN_I]], %[[C1]] : index
// CHECK: %[[CORE_REMAIN_J:[A-Za-z0-9_]+]] = arith.subi %c8, %[[BLOCK_J]] : index
// CHECK: %[[CORE_SIZE_J:[A-Za-z0-9_]+]] = arith.minui %[[CORE_REMAIN_J]], %[[C1]] : index
// CHECK: %[[CAN_I:[A-Za-z0-9_]+]] = arith.cmpi uge, %[[BLOCK_I]], %[[C1]] : index
// CHECK: %[[LEFT_I:[A-Za-z0-9_]+]] = arith.subi %[[BLOCK_I]], %[[C1]] : index
// CHECK: %[[HALO_I:[A-Za-z0-9_]+]] = arith.select %[[CAN_I]], %[[LEFT_I]], %c0 : index
// CHECK: %[[GROW_I:[A-Za-z0-9_]+]] = arith.subi %[[BLOCK_I]], %[[HALO_I]] : index
// CHECK: %[[WANT_I0:[A-Za-z0-9_]+]] = arith.addi %[[GROW_I]], %[[CORE_SIZE_I]] : index
// CHECK: %[[WANT_I:[A-Za-z0-9_]+]] = arith.addi %[[WANT_I0]], %[[C1]] : index
// CHECK: %[[ROOM_I:[A-Za-z0-9_]+]] = arith.subi %c8, %[[HALO_I]] : index
// CHECK: %[[HALO_SIZE_I:[A-Za-z0-9_]+]] = arith.minui %[[ROOM_I]], %[[WANT_I]] : index
// CHECK: %[[CAN_J:[A-Za-z0-9_]+]] = arith.cmpi uge, %[[BLOCK_J]], %[[C1]] : index
// CHECK: %[[LEFT_J:[A-Za-z0-9_]+]] = arith.subi %[[BLOCK_J]], %[[C1]] : index
// CHECK: %[[HALO_J:[A-Za-z0-9_]+]] = arith.select %[[CAN_J]], %[[LEFT_J]], %c0 : index
// CHECK: %[[GROW_J:[A-Za-z0-9_]+]] = arith.subi %[[BLOCK_J]], %[[HALO_J]] : index
// CHECK: %[[WANT_J0:[A-Za-z0-9_]+]] = arith.addi %[[GROW_J]], %[[CORE_SIZE_J]] : index
// CHECK: %[[WANT_J:[A-Za-z0-9_]+]] = arith.addi %[[WANT_J0]], %[[C1]] : index
// CHECK: %[[ROOM_J:[A-Za-z0-9_]+]] = arith.subi %c8, %[[HALO_J]] : index
// CHECK: %[[HALO_SIZE_J:[A-Za-z0-9_]+]] = arith.minui %[[ROOM_J]], %[[WANT_J]] : index
// CHECK: arts.db_acquire[<in>] {{.*}} offsets[%[[HALO_I]], %[[HALO_J]]], sizes[%[[HALO_SIZE_I]], %[[HALO_SIZE_J]]]
// CHECK-SAME: element_offsets[%c0, %c0, %c0, %c0]
// CHECK-SAME: element_sizes[%c1, %c1, %c32, %c32]
// CHECK-SAME: haloViewDependency
// CHECK-SAME: stencil_supported_block_halo
// CHECK: arts.edt <task> <intranode> route{{.*}} params({{.*}}%[[HALO_I]], %[[HALO_J]],{{.*}} attributes {{.*}}perBlockHaloExchange{{.*}}stencil_supported_block_halo
// CHECK-NEXT: ^bb0(%{{arg[0-9]+}}: {{[^,]*}}, %[[HALO_DEP:arg[0-9]+]]: {{[^,]*}}, %{{arg[0-9]+}}: {{[^,]*}}, %{{arg[0-9]+}}: index, %{{arg[0-9]+}}: index, %{{arg[0-9]+}}: index, %{{arg[0-9]+}}: index, %[[HALO_ARG_I:arg[0-9]+]]: index, %[[HALO_ARG_J:arg[0-9]+]]: index
// CHECK-NOT: arts.db_ref %[[HALO_DEP]][%c0, %c0]
// CHECK: %[[LOCAL_I:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %[[HALO_ARG_I]] : index
// CHECK: %[[LOCAL_J:[A-Za-z0-9_]+]] = arith.subi %{{[A-Za-z0-9_]+}}, %[[HALO_ARG_J]] : index
// CHECK: %[[HALO_BLOCK:[A-Za-z0-9_]+]] = arts.db_ref %[[HALO_DEP]][%[[LOCAL_I]], %[[LOCAL_J]]]
// CHECK: memref.load %[[HALO_BLOCK]]
