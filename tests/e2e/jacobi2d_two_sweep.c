// RUN: %carts compile %samples_dir/stencil/jacobi2d_two_sweep.c -O3 --arts-config %arts_config -o %t_arts
// RUN: env artsConfig=%arts_config %t_arts | %FileCheck %s
// CHECK: [CARTS] {{.*}}: PASS
