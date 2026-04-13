# RFC: `sde.cu_codelet` + `RaiseMemrefToTensor` — Dataflow-driven SDE

**Status:** Draft — proposed for the SDE pipeline.
**Owner:** TBD.
**Pairs with:** `docs/sde.md`, `docs/compiler/pipeline.md`.

## 1. Motivation

The SDE pipeline today lowers OpenMP into a **memref-fallback** form:
shared variables remain `memref<KxT>` allocas inherited from Polygeist,
and cross-task dependencies are declared via `sde.mu_dep` alongside
in-body `memref.load` / `memref.store`. Dependencies live partly in the
op structure, partly in the body, and partly in address-based runtime
tracking.

The dep path is fragile. `samples/deps/deps.c` is the canonical failure:
Task 1 writes `a` but has no `mu_dep` — its write is a `memref.store`
inside the body. Task 2 declares `<read> %a`, but any downstream pass
that loses the address-based linkage between Task 1's store and Task 2's
acquire silently drops the dep. Task 2 reads `a=0` instead of the random
value Task 1 produced.

This is structural. A pipeline that carries dep information through
addresses will drop it whenever a transform doesn't model address
identity. The fix is to make the dependency graph **SSA-visible**: every
task's inputs/outputs are explicit SSA operands/results, so dropping a
dep becomes dropping an SSA use-def edge — which the MLIR verifier
refuses to accept.

The `SdeMuDepOp` docstring already anticipates this:

> Used in the memref fallback path. In the tensor path,
> `tensor.extract_slice` / `tensor.insert_slice` carry the same
> information natively.

The tensor path was never wired up for tasks. This RFC fills the gap by
introducing a **separate, strictly-isolated task op** and a transform
that mechanically converts non-isolated tasks into isolated ones.

## 2. Prior art

| Project | Pass / concept | Applicable? |
| --- | --- | --- |
| MLIR upstream | `one-shot-bufferize`, `-func-bufferize`, etc. | Opposite direction (tensor → memref). No general reverse. |
| MLIR upstream | `bufferization.to_tensor` / `to_buffer` ops | Per-value conversion ops. No SSA-threading pass that raises whole memref allocations. |
| MLIR upstream | `-detensorize` | 0-D tensors → scalars. Narrow, opposite intent. |
| LLVM | `mem2reg` | Philosophical match — promote memory-backed state to SSA. Scalar-only. This work is the tensor-typed generalization over MLIR regions. |
| CARTS | `RaiseToLinalg` | Targets `sde.su_iterate` bodies only (loops). Does not raise memrefs generally. |
| CARTS | `RaiseToTensor` | Rewrites emitted `linalg.generic(memref) → linalg.generic(tensor)`. Operates on linalg carriers, not arbitrary SDE data. |
| Polygeist | `RaiseMemRefDimensionality`, `RaiseSCFToAffine` | Shape / control-flow raising. No memref → tensor. |

**Conclusion:** the transformation we need does not exist in MLIR
upstream or anywhere we surveyed. It is a **tensor-typed Mem2Reg across
an SDE region**, with `sde.cu_codelet` as the isolated dataflow unit
and `scf.for` / `scf.if` iter_args for control flow.

## 3. Design overview

Two task forms, one transform between them:

1. `sde.cu_task` — **permissive**. Existing op, unchanged. Allows
   external values: captures enclosing SSA inside its body, references
   memrefs from outer scope, makes direct `memref.load` / `memref.store`
   calls on them. Matches today's OMP-lowering output from
   `ConvertOpenMPToSde`.
2. `sde.cu_codelet` — **forces dataflow style**. New op carrying
   `IsolatedFromAbove` plus a `NoHiddenSideEffects` contract. The body
   cannot reference any SSA value outside its own block arguments. All
   inputs come in as operands, all outputs flow out through
   `sde.yield`. Name follows OCR's "codelet" — a pure-dataflow unit of
   asynchronous work.
3. `RaiseMemrefToTensor` — transform pass. Converts `cu_task` →
   `cu_codelet` by threading every external value the body reads into
   operands, every external write into results, and raising the
   relevant memrefs to tensor SSA along the way. Runs inside the
   `openmp-to-arts` stage.

The split lets front-end lowering stay simple (keep emitting
`cu_task`) while giving the middle-end a strong target form where
correctness is type-checked.

## 4. IR additions

### 4.1 `sde.cu_codelet` — dataflow-isolated compute unit

```tablegen
def SdeCuCodeletOp : ArtsSde_Op<"cu_codelet", [
      IsolatedFromAbove,
      SingleBlock,
      RecursiveMemoryEffects,
      AttrSizedOperandSegments
    ]> {
  let summary = "Dataflow codelet: all I/O through operands/results, no captures.";
  let description = [{
    A compute unit whose only communication with the enclosing scope is
    through its operands and yielded results. The name follows OCR's
    "codelet" — a purely asynchronous, pure-dataflow unit of work whose
    inputs and outputs are entirely explicit.

    `IsolatedFromAbove` guarantees the body references no SSA value from
    outside its block arguments. Any external memref the source task
    read becomes a tensor operand; any external memref it wrote becomes
    a tensor result. Effects that are observable to the surrounding
    program flow through `sde.yield`.

    Differences from `sde.cu_task`:
    - No implicit captures of enclosing SSA values.
    - No hidden side effects on shared memrefs; observable effects are
      expressed through `sde.yield` results.
    - Dependencies are SSA-visible: a missing dep is a missing operand,
      caught by the verifier.
  }];

  let arguments = (ins
    Variadic<AnyType>:$operands,
    Variadic<SdeDepType>:$deps        // kept for backward-compat bridging
  );
  let results   = (outs Variadic<AnyType>:$outputs);
  let regions   = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    (`deps` `(` $deps^ `:` type($deps) `)`)?
    `(` ($operands^ `:` type($operands))? `)`
    `->` `(` type($outputs) `)`
    $body attr-dict
  }];
}
```

Block arguments match `$operands` 1:1 by type. The terminator is
`sde.yield` with values bound to `$outputs` by position.

### 4.2 `sde.mu_data` — declarative SDE data handle

```tablegen
def SdeMuDataOp : ArtsSde_Op<"mu_data", [Pure]> {
  let summary = "Declarative SDE shared-data handle.";
  let description = [{
    Canonical anchor for data that crosses SDE tasks. Replaces the
    pattern of inheriting Polygeist `memref.alloca` for OMP-shared
    variables. Produced by `ConvertOpenMPToSde` for every variable named
    by an OMP `shared(...)` clause (or otherwise shared across tasks),
    consumed by `RaiseMemrefToTensor` as an enumeration anchor, lowered
    by `ConvertSdeToArts` to `arts.db_alloc`.
  }];
  let arguments = (ins
    OptionalAttr<AnyAttr>:$init,    // optional compile-time initializer
    OptionalAttr<UnitAttr>:$shared
  );
  let results = (outs AnyType:$handle);   // tensor<...> or memref<...>
  let assemblyFormat = "attr-dict `:` type($handle)";
}
```

### 4.3 Boundary glue

Use upstream `bufferization.to_tensor` (memref → tensor view) and
`bufferization.to_buffer` (tensor → memref store) at the boundary where
a raised region meets non-raised or non-SDE code. No dialect-local
boundary ops needed.

## 5. `RaiseMemrefToTensor` — the transform pass

**Name:** `arts-sde-raise-memref-to-tensor`.

**Placement** (inside stage 3, `openmp-to-arts`):

```text
ConvertOpenMPToSde
  → ScopeSelection
  → ScheduleRefinement
  → ChunkOpt
  → ReductionStrategy
  → RaiseMemrefToTensor              ← NEW
  → RaiseToLinalg                     (now consumes tensors natively)
  → RaiseToTensor                     (becomes a narrow cleanup, eventually retires)
  → LoopInterchange → TensorOpt → StructuredSummaries → …
  → ConvertSdeToArts
```

### 5.1 What it does

For every `sde.cu_task` whose body reads or writes memrefs from the
enclosing scope (the common case):

1. **Collect raisable memrefs.** A memref is raisable iff
   - not captured by any `call @extern(...)`;
   - not aliased (no `memref.subview` / `memref.cast` producing distinct
     handles that are later stored or loaded);
   - all accesses are analyzable: constant-indexed loads/stores, or
     recognized slice patterns (`tensor.extract_slice` /
     `tensor.insert_slice` shape);
   - type is statically ranked.
   Memrefs failing any check stay on the memref-fallback path; the pass
   skips them and moves on.

2. **Build tensor initializers.** Each raisable memref gets a
   function-scope `tensor.empty` (or `tensor.from_elements` for a known
   initializer). Prior `memref.store %c0, %m[%c0]` acts as a recognized
   zero-initializer and is erased.

3. **Type-lift region signatures.** For every `sde.cu_region` /
   `sde.su_iterate` / `scf.for` / `scf.if` that transitively touches a
   raisable memref, extend operands/results/iter_args to carry the
   corresponding tensor values.

4. **Rewrite the `cu_task` op** to `cu_codelet`:
   - Compute the set of external memrefs used inside the body.
   - Partition uses into read-only / write-only / read-modify-write.
   - Add tensor operands for reads, tensor results for writes (rmw gets
     both).
   - Rewrite the body:
     - `memref.load %m[%i…]` → `tensor.extract %arg[%i…] : tensor<…>`
     - `memref.load` of a slice → `tensor.extract_slice …`
     - `memref.store %v, %m[%i…]` → `%t' = tensor.insert %v into %cur[%i…]`
       (maintain a running `current_tensor[memref]` map)
     - `memref.store` of a slice → `tensor.insert_slice …`
   - Replace the terminator with `sde.yield` of the final tensor values.
   - Because the op is now `IsolatedFromAbove`, remaining references to
     enclosing SSA are either legal (they are the new operands) or an
     error caught by the verifier.

5. **Thread through control flow.** `scf.for` / `scf.while` add
   `iter_args` for each raised memref; `scf.if` merges branch-yielded
   tensors. Tensors flow naturally as SSA.

6. **Boundary materialization.** At each point where the raised
   memref's SSA value leaves the pass's scope (function exit, external
   call receiving the original memref, unraised consumer), insert
   `tensor.extract` + `memref.store` back into the original memref. On
   entry from outside, insert `bufferization.to_tensor %m_original` at
   the top of the raising region.

7. **Erase `mu_dep`** ops whose memref was raised. Non-raised memrefs
   continue using `mu_dep`.

### 5.2 Preconditions (rollout scope)

Three releases. Each is self-contained and testable.

- **v1:** `memref<KxT>` with small constant `K`, constant-index accesses
  only. Covers `deps.c`, scalar reductions, small aggregate tasks.
- **v2:** Any statically-ranked memref with slice-shaped accesses.
  Covers stencils and matrix-tile patterns.
- **v3:** Dynamic-ranked memrefs with conservative alias analysis.

Memrefs outside the active scope fall back to memref + `mu_dep`. The
pass is strictly additive across versions — it never breaks programs it
cannot raise.

### 5.3 Correctness invariants

- **SSA preservation.** After the pass, every raised memref is a valid
  SSA chain of tensor values. The MLIR verifier rejects a dropped dep.
- **Observable equivalence at boundaries.** The value of the original
  memref after the raising scope equals what it was before.
- **No speculation.** If the pass cannot prove a raise is safe, the
  memref stays on the fallback path. Mixed raised/non-raised modules
  are valid and compile correctly.
- **Idempotence.** Running the pass twice is a no-op on already-raised
  data.

## 6. Lowering story (SDE → ARTS)

`ConvertSdeToArts` gains two small branches:

- **Tensor-typed `mu_data`** → `arts.db_alloc` with the tensor's shape.
  SSA users become `arts.db_acquire <read | write | readwrite>` based
  on whether they are operand uses (read) or task-result producers
  (write).
- **`sde.cu_codelet`** → `arts.edt` whose operand DBs come from the
  SSA operands' producing DBs and whose output DB comes from the tensor
  result's SSA user chain. The GUID routing is direct — each SSA edge
  maps 1:1 to a DB dependency edge. `sde.cu_task` continues to lower as
  today.

No change to `arts.edt`, `arts.db_alloc`, or RT lowering. Tensor lifting
is entirely an SDE-side concern; by the time we reach ARTS core, DBs
and EDTs look the same as today — just produced by a more principled
front-end for raised code.

## 7. Interactions with existing passes

| Pass | Today | After this RFC |
| --- | --- | --- |
| `RaiseToLinalg` | Walks `su_iterate` bodies, emits `linalg.generic(memref)` when the pattern matches. | Walks `su_iterate` bodies, emits `linalg.generic(tensor)` directly — no memref middle step. |
| `RaiseToTensor` | Rewrites emitted `linalg.generic(memref)` → `linalg.generic(tensor)`. | Becomes a narrow cleanup for cases that escaped the main raise. Retires when v3 lands. |
| `ScopeSelection`, `ScheduleRefinement`, `ChunkOpt`, `ReductionStrategy` | Annotate SDE ops. | Unchanged. Run before the raise, so SDE ops still carry memref form at annotation time. |
| `LoopInterchange`, `TensorOpt`, `StructuredSummaries`, `ElementwiseFusion`, `DistributionPlanning`, `IterationSpaceDecomposition` | Mixed memref/tensor. | Uniform tensor — cleaner cost models, fewer special cases. |
| `BarrierElimination` | Inspects `mu_dep` / `su_barrier`. | `mu_dep` count drops for raised memrefs; tensor SSA makes the dep graph directly visible. |
| `ConvertSdeToArts` | Memref path via `mu_dep`. | Adds isolated-task branch (SSA operands → `db_acquire`, tensor results → `db_alloc` + `db_acquire`). Fallback memref branch unchanged. |

## 8. Rollout

Each step is self-contained:

1. **Op extensions.** Add `sde.cu_codelet` and `sde.mu_data`. No uses
   yet. Existing tests pass unchanged.
2. **Lowering support in `ConvertSdeToArts`.** Handle the new ops with
   hand-written MLIR regression tests.
3. **`RaiseMemrefToTensor` v1** (scalar / small constant memrefs).
   Validate with `samples/deps/deps.c`, `samples/parallel/parallel.c`,
   and new lit tests under
   `lib/arts/dialect/sde/test/raise-memref-to-tensor/`.
4. **Memref-path dep-drop fix.** Keep the fallback correct while v1
   only handles a subset; `parallel_for/*` still depends on it.
5. **v2** (slice access). Validate on `parallel_for/stencil`,
   `matrixmul`, `jacobi/*`.
6. **v3** (dynamic rank, conservative aliasing).
7. **Retire `RaiseToTensor`** once v3 covers the use cases.

## 9. Worked example — `samples/deps/deps.c`

See `samples/deps/pipelines/2_arts/boundaries/01_omp_to_sde/after.mlir`
for the current form. After `RaiseMemrefToTensor` (expected shape):

```mlir
%a_init = tensor.from_elements %c0_i32 : tensor<i32>
%b_init = tensor.from_elements %c0_i32 : tensor<i32>

%a_final, %b_final = arts_sde.cu_region <parallel>
    (%a_in = %a_init, %b_in = %b_init : tensor<i32>, tensor<i32>)
    -> (tensor<i32>, tensor<i32>) {
  %a_s, %b_s = arts_sde.cu_region <single> scope(<local>)
      (%as = %a_in, %bs = %b_in : tensor<i32>, tensor<i32>)
      -> (tensor<i32>, tensor<i32>) {

    %a_t1 = arts_sde.cu_codelet() -> (tensor<i32>) {
      %r = func.call @rand() : () -> i32
      %v = arith.remsi %r, %c100_i32 : i32
      %t = tensor.from_elements %v : tensor<i32>
      arts_sde.yield %t : tensor<i32>
    }

    %b_t2 = arts_sde.cu_codelet(%a_t1 : tensor<i32>) -> (tensor<i32>) {
    ^bb0(%ai : tensor<i32>):
      %va = tensor.extract %ai[] : tensor<i32>
      %nb = arith.addi %va, %c5_i32 : i32
      %t  = tensor.from_elements %nb : tensor<i32>
      arts_sde.yield %t : tensor<i32>
    }

    arts_sde.cu_codelet(%b_t2 : tensor<i32>) -> () {
    ^bb0(%bi : tensor<i32>):
      %vb = tensor.extract %bi[] : tensor<i32>
      // … printf …
      arts_sde.yield
    }

    arts_sde.yield %a_t1, %b_t2 : tensor<i32>, tensor<i32>
  }
  arts_sde.yield %a_s, %b_s : tensor<i32>, tensor<i32>
}

%a_final_scalar = tensor.extract %a_final[] : tensor<i32>
%b_final_scalar = tensor.extract %b_final[] : tensor<i32>
memref.store %a_final_scalar, %alloca_0[%c0] : memref<1xi32>
memref.store %b_final_scalar, %alloca[%c0]   : memref<1xi32>
```

Every task is `cu_codelet`. Task 2's dep on Task 1 is the SSA operand
`%a_t1`. No `mu_dep` remains. Task 1 no longer has a hidden write —
its output is an SSA result the verifier tracks. Dropping any dep
requires detaching a use-def edge, which the verifier refuses.

## 10. Open questions

1. **`mu_data` vs. bare `tensor.empty`.** Is the dialect-specific
   declarative op worth it, or does SSA + `tensor.empty` carry enough?
   Leaning toward `mu_data` when we need attributes (initial value,
   partitioning hint, shared-mode marker); bare `tensor.empty`
   otherwise.

2. **Scope of `IsolatedFromAbove`.** Is strict isolation too aggressive
   for intermediate SDE stages that still want to reference outer
   region-scope SSA? Mitigation: run the pass late enough that all
   capture-required transforms have happened; everything downstream of
   the raise operates on isolated tasks only.

3. **Reductions.** `cu_reduce` already has tensor-ish semantics via its
   accumulator/partial/identity operands. The raised form of a
   reduction loop should line up with what `cu_reduce` expects. May
   need a small adapter.

4. **Bufferization at the SDE→ARTS boundary.** Keeping tensor types
   end-to-end in SDE and bufferizing at the SDE→ARTS boundary is
   cleaner than today's partial-bufferize story. Likely a single
   one-shot-bufferize call in `ConvertSdeToArts`.

5. **Default-on or opt-in.** Propose opt-in via
   `-sde-raise-memref-to-tensor` for one cycle so we can A/B the sample
   suite and benchmarks, then flip on by default.

## 11. Success criteria

- `samples/deps/deps.c` passes after v1 lands, without special-case
  patches in lower stages.
- No regression in sample-suite pass rate.
- `ConvertSdeToArts` output for `deps`, `parallel_for/reduction`,
  `jacobi/deps` shows SSA-visible DB handoffs between EDTs — no hidden
  address-based dep reconstruction.
- `RaiseToTensor` usage drops to zero across the sample suite after
  v3.
