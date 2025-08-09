set(LLVM_TARGET_DEFINITIONS ArtsOps.td)
mlir_tablegen(ArtsOpsEnums.h.inc -gen-enum-decls)
mlir_tablegen(ArtsOpsEnums.cpp.inc -gen-enum-defs)
mlir_tablegen(ArtsOpsAttributes.h.inc -gen-attrdef-decls
              -attrdefs-dialect=arts)
mlir_tablegen(ArtsOpsAttributes.cpp.inc -gen-attrdef-defs
              -attrdefs-dialect=arts)
add_mlir_dialect(ArtsOps arts)

add_subdirectory(Passes)

# Runtime TD (for future codegen consumers)
set(LLVM_TARGET_DEFINITIONS Codegen/ArtsRuntime.td)
mlir_tablegen(ArtsRuntime.inc -gen-enum-defs) # placeholder to ensure build integration