# C++ Style

CARTS style should make ownership and contract surfaces easy to inspect. Prefer
small namespaces, generated accessors, and reusable helpers over header-level
using directives, static-only god classes, or hand-rolled op-type switches.

## Evidence

- Polygeist public headers define declarations inside namespaces:
  `external/Polygeist/include/polygeist/Passes/Passes.h:25`,
  `external/Polygeist/include/polygeist/Passes/Passes.h:29`,
  `external/Polygeist/include/polygeist/Passes/Passes.h:90`.
- Polygeist implementation files keep `using namespace` local to `.cpp` files:
  `external/Polygeist/lib/polygeist/Dialect.cpp:13`,
  `external/Polygeist/lib/polygeist/Ops.cpp:39`,
  `external/Polygeist/lib/polygeist/Passes/PolygeistCanonicalize.cpp:38`.
- ODS declarations expose arguments, results, interfaces, and generated hooks as
  the canonical API:
  `external/Polygeist/include/polygeist/PolygeistOps.td:139`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:143`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:251`,
  `external/Polygeist/include/polygeist/PolygeistOps.td:252`.
- SCF tests compare `OpFoldResult` values through helper APIs rather than
  inlining constant checks everywhere:
  `external/Polygeist/llvm-project/mlir/unittests/Dialect/SCF/LoopLikeSCFOpsTest.cpp:105`,
  `external/Polygeist/llvm-project/mlir/unittests/Dialect/SCF/LoopLikeSCFOpsTest.cpp:107`,
  `external/Polygeist/llvm-project/mlir/unittests/Dialect/SCF/LoopLikeSCFOpsTest.cpp:108`.

## CARTS Rules

- Headers: no broad `using namespace`; expose typed APIs and generated names.
- Implementation files: local `using namespace` is fine when it reduces noise.
- Static helpers: keep pass-local when truly local; move reusable dialect logic
  to the narrowest `Utils/` home.
- Comments: keep invariant and boundary notes; remove history and restatements.
- Attribute strings: prefer ODS-generated accessors and central attr-name
  surfaces; raw strings are migration bridges only.
