// RUN: sh -c 'CARTS_COMPILE_WORKDIR=%t.compile %carts compile %S/../../external/carts-benchmarks/polybench/seidel-2d/seidel-2d.c --pipeline arts-to-llvm --arts-config %S/inputs/arts_64t.cfg -- -I%S/../../external/carts-benchmarks/polybench/seidel-2d -I%S/../../external/carts-benchmarks/polybench/common -I%S/../../external/carts-benchmarks/polybench/utilities -DTSTEPS=320 -DN=9600 -lm >/dev/null && cat %t.compile/seidel-2d.arts-to-llvm.mlir' | %FileCheck %s

// Verify that widened Seidel wavefront inout dependencies lower to runtime
// face slices: neighbor blocks become RO slice transports that preserve the
// original block coordinate system, while the owned center block dynamically
// falls back to the whole-DB dependence path.
// CHECK-DAG: func.func private @arts_add_dependence_ex(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i32 {llvm.zeroext})
// CHECK-DAG: func.func private @arts_add_dependence_at_ex(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i64, i64, i32 {llvm.zeroext})
// CHECK-NOT: func.func private @arts_add_dependence_at(i64, i64, i32 {llvm.zeroext}, i32 {llvm.signext}, i64, i64)
// CHECK: %[[ROW_BYTES:.+]] = arith.muli %{{.+}}, %c76800 : index
// CHECK: %[[BYTE_SIZE:.+]] = arith.select %{{.+}}, %c0_i64, %{{.+}} : i64
// CHECK: %[[USE_WHOLE:.+]] = arith.cmpi eq, %[[BYTE_SIZE]], %c0_i64 : i64
// CHECK: scf.if %[[USE_WHOLE]] {
// CHECK: func.call @arts_add_dependence_ex({{.*}}%c2_i32) : (i64, i64, i32, i32, i32) -> ()
// CHECK: } else {
// CHECK: %[[BYTE_OFF:.+]] = arith.select %{{.+}}, %c0_i64, %{{.+}} : i64
// CHECK: func.call @arts_add_dependence_at_ex({{.*}}%[[BYTE_OFF]], %[[BYTE_SIZE]], %c2_i32) : (i64, i64, i32, i32, i64, i64, i32) -> ()
// CHECK: }

module {
}
