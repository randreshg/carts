set(LLVM_TARGET_DEFINITIONS ArtsPasses.td)
mlir_tablegen(ArtsPasses.h.inc -gen-pass-decls -name Arts)
add_public_tablegen_target(MLIRArtsPassIncGen)
