// RUN: %carts compile %samples_dir/jacobi/for/jacobi-for.c -O3 --distributed-db --arts-config %arts_multinode_config -o %t_arts
// RUN: env ARTS_CONFIG=%arts_multinode_config artsConfig=%arts_multinode_config %t_arts | %FileCheck %s
// CHECK: [CARTS] for: PASS
