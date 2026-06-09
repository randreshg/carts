// RUN: %carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 --arts-config %arts_config -o %t_arts_8t
// RUN: env artsConfig=%arts_config %t_arts_8t | %FileCheck %s
// RUN: %carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 -DSIZE=256 --arts-config %inputs_dir/arts_64t.cfg -o %t_arts_64t
// RUN: env artsConfig=%inputs_dir/arts_64t.cfg %t_arts_64t | %FileCheck %s

// CHECK: [CARTS] for: PASS
