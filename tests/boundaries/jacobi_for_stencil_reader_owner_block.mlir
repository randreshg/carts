// RUN: rm -rf %t.dir && mkdir -p %t.dir
// RUN: cd %S/../.. && env CARTS_COMPILE_WORKDIR=%t.dir .dekk/env/bin/dekk carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 \
// RUN:   --arts-config %inputs_dir/arts_64t.cfg --pipeline=sde-to-codir
// RUN: %FileCheck %s < %t.dir/jacobi-for.sde-to-codir.mlir

// The double-buffered Jacobi stencil and the full-timestep copy that feeds it
// must expose compute-block dependency views at the same DB/MU grain as the
// writer. CODIR-to-ARTS must consume dep_storage_views, dep_owner_dims,
// subviews, and halo windows as the storage authority.

// CHECK: memref.subview {{.*}} [1, 1, 32, 32] [1, 1, 1, 1]
// CHECK: codir.codelet deps({{.*}}memref<1x1x32x32xf64
// CHECK-SAME: dep_modes = [#codir.access_mode<write>, #codir.access_mode<read>]
// CHECK-SAME: dep_owner_dims = {{\[\[0, 1\], \[0, 1\]\]}}
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>]
// CHECK-SAME: logical_worker_slice = [32, 32]
// CHECK-SAME: pattern = #codir.pattern<uniform>
// CHECK-SAME: tile_shape = [32, 32]

// CHECK: memref.subview {{.*}} [1, 1, 32, 32] [1, 1, 1, 1]
// CHECK: dep_collectives = [#codir.collective<none>, #codir.collective<halo>, #codir.collective<none>]
// CHECK-SAME: dep_modes = [#codir.access_mode<read>, #codir.access_mode<read>, #codir.access_mode<write>]
// CHECK-SAME: dep_owner_dims = {{\[\[0, 1\], \[0, 1\], \[0, 1\]\]}}
// CHECK-SAME: dep_storage_views = [#codir.storage_view<compute_block>, #codir.storage_view<compute_block>, #codir.storage_view<compute_block>]
// CHECK-SAME: halo_shape = [1, 1]
// CHECK-SAME: logical_worker_slice = [32, 32]
// CHECK-SAME: pattern = #codir.pattern<alternating_buffer_stencil>
// CHECK-SAME: tile_shape = [32, 32]
