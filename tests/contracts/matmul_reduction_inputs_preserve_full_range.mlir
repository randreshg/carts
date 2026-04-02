// RUN: sh -c '%carts-compile %S/Output/matmul_update_tiling_2mm.mlir.tmp.compile/2mm.mlir --pipeline db-partitioning --arts-config %S/inputs/arts_64t.cfg -o %t.2mm >/dev/null && cat %t.2mm' | %FileCheck %s --check-prefix=TWOMM
// RUN: sh -c '%carts-compile %S/Output/matmul_update_tiling_3mm.mlir.tmp.compile/3mm.mlir --pipeline db-partitioning --arts-config %S/inputs/arts_64t.cfg -o %t.3mm >/dev/null && cat %t.3mm' | %FileCheck %s --check-prefix=THREEMM

// Matmul reduction inputs that do not follow the distributed owner dimension
// must remain whole-range after db-partitioning. Owner-local row inputs can
// still keep their block-local element slice.

// TWOMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>), offsets[%c0{{.*}}], sizes[%c1] {
// TWOMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>), offsets[%c0{{.*}}], sizes[%c1] {
// TWOMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>, offsets[%{{.*}}], sizes[%c4]), offsets[%{{.*}}], sizes[%c1{{.*}}] element_offsets[%{{.*}}] element_sizes[%c4] {

// THREEMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>), offsets[%c0{{.*}}], sizes[%c1] {
// THREEMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<coarse>), offsets[%c0{{.*}}], sizes[%c1] {
// THREEMM-DAG: arts.db_acquire[<in>] ({{.*}}) partitioning(<block>, offsets[%{{.*}}], sizes[%c4]), offsets[%{{.*}}], sizes[%c1{{.*}}] element_offsets[%{{.*}}] element_sizes[%c4] {
