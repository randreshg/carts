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

Two task forms, one tokenized access contract between them, and one
transform pass that ties them together:

1. `sde.cu_task` — **permissive**. Existing op, unchanged. Allows
   external values: captures enclosing SSA inside its body, references
   memrefs from outer scope, makes direct `memref.load` /
   `memref.store` calls on them. Matches today's OMP-lowering output
   from `ConvertOpenMPToSde`.
2. `sde.cu_codelet` — **forces dataflow style**. New op carrying
   `IsolatedFromAbove` plus a `NoHiddenSideEffects` contract. Operands
   are **`!sde.token` values**, not raw tensors; each token names a
   (sub)region of a tensor with an explicit access mode (`read` /
   `write` / `readwrite`). All outputs flow out through `sde.yield`.
   The name follows OCR's "codelet" — a pure-dataflow unit of
   asynchronous work.
3. `!sde.token` + `sde.mu_token` — **tensor-path access contract**.
   The token is a handle to a whole tensor or a rectangular slice,
   carrying a mode attribute. It is the tensor-world analogue of the
   existing `sde.mu_dep` on memrefs, and lowers 1:1 to
   `arts.db_acquire` at the SDE→ARTS boundary.
4. `RaiseMemrefToTensor` — transform pass. Converts `cu_task` →
   `cu_codelet` by raising the relevant memrefs to tensors, cutting
   the access surface of the body into `sde.mu_token` operands, and
   yielding updated parent tensors via `sde.yield`. Runs inside the
   `openmp-to-arts` stage.

The split lets front-end lowering stay simple (keep emitting
`cu_task` and `mu_dep`) while giving the middle-end a strong target
form where correctness and parallelism granularity are both
type-checked.

### 3.1 Naming conventions

All new ops follow the existing SDE scheme, which segregates ops by
structural role via the second identifier:

| Prefix | Role | Existing members | New members |
| --- | --- | --- | --- |
| `cu_` | Compute unit — does work | `cu_region`, `cu_task`, `cu_reduce`, `cu_atomic` | `cu_codelet` |
| `su_` | Scheduling unit — controls iteration | `su_iterate`, `su_distribute`, `su_barrier` | — |
| `mu_` | Memory unit — describes data or access | `mu_dep`, `mu_reduction_decl` | `mu_data`, `mu_token` |

Types are plain nouns under the dialect namespace, matching existing
precedent:

| Type | Role | Parallel to |
| --- | --- | --- |
| `!sde.dep` | Result of `mu_dep` — memref access handle | (existing) |
| `!sde.token` | Result of `mu_token` — tensor access handle with `slice_type` parameter | tensor-world analogue of `!sde.dep` |

Name-by-name rationale:

- **`cu_codelet`.** OCR lineage — ARTS inherits OCR's asynchronous
  model, and "codelet" is OCR's established term for a purely
  dataflow unit of work. Reads naturally against `cu_task`
  (permissive) without the awkward double-barrel of `cu_iso_task` or
  the misleading generality of `cu_pure`. Distinct from `sde.cu_task`
  at a glance.
- **`mu_token`.** Mirrors `mu_dep` exactly — same shape (`<mode>
  source [offsets] size [sizes]`), same role (declarative access
  contract), different domain (tensor vs memref). Anyone who has read
  `mu_dep` understands `mu_token` in one pass. The common `mu_` prefix
  makes the memref / tensor duality obvious across the dialect.
- **`!sde.token`.** `async.token` in upstream MLIR denotes an async
  future, not a data handle; the `sde.` dialect prefix in the printed
  form (`!arts_sde.token`) is enough separation. Keeping the plain
  noun matches `!sde.dep` and reads fluidly: "produce a token for
  this slice, consume it in a codelet".
- **`mu_data`.** Declarative anchor for SDE-owned data, following
  `mu_reduction_decl`'s pattern of declaring MU-scoped entities. Drop
  the `_decl` suffix because `mu_data` is a value-producing op, not a
  module-level symbol (`mu_reduction_decl` is an `IsolatedFromAbove`
  `Symbol`, structurally different).
- **`RaiseMemrefToTensor`.** Matches the existing `Raise*` family
  (`RaiseToLinalg`, `RaiseToTensor`, `RaiseMemRefDimensionality`). TD
  record name is unprefixed like its siblings; pass CLI is
  `-arts-sde-raise-memref-to-tensor`.

Dialect-prefix rendering:

- **MLIR printed form:** `arts_sde.X` (full dialect id).
- **Prose shorthand:** `sde.X` (brief, consistent with `docs/sde.md`
  and `docs/architecture/pass-placement.md`).
- **C++ namespace:** `mlir::arts::sde`.

Best-practices checklist applied:

- Snake_case op names, nouns or noun-phrases.
- Variadic operand lists use `Variadic<T>` with `AttrSizedOperandSegments`
  only when multiple variadic groups coexist.
- Traits match semantics (`IsolatedFromAbove`, `Pure`,
  `RecursiveMemoryEffects`) — no ad-hoc correctness attributes.
- Verifier rules are stated in the op description, implemented in the
  verifier, and listed in §4.4 as explicit invariants (V1–V13).

## 4. IR additions

### 4.1 `!sde.token` type

```tablegen
def SdeTokenType : ArtsSde_Type<"Token", "token"> {
  let summary = "Handle to a (sub)region of a shared tensor.";
  let description = [{
    A token names a subset of a tensor's storage with a declared access
    mode. It is the tensor-world analogue of `!sde.dep`: carries the
    same "mode + region" information that `sde.mu_dep` carries for
    memrefs, but with tensor SSA for the underlying value.

    The `slice_type` parameter records the shape of the addressed
    region so codelet block arguments can be typed directly.
  }];
  let parameters = (ins "RankedTensorType":$slice_type);
  let assemblyFormat = "`<` $slice_type `>`";
}
```

### 4.2 `sde.mu_token` — access token producer

```tablegen
def SdeMuTokenOp : ArtsSde_Op<"mu_token", [Pure, AttrSizedOperandSegments]> {
  let summary = "Produce a tensor-path access token (whole or slice).";
  let description = [{
    Names a tensor (or a rectangular slice of one) for consumption by a
    `sde.cu_codelet`, with an explicit access mode. When `offsets`/
    `sizes` are omitted the token addresses the whole tensor.

    Tokens are the tensor-path analogue of `sde.mu_dep`. At the SDE→
    ARTS boundary a `mu_token` lowers 1:1 to `arts.db_acquire <mode>`.

    Two tokens with disjoint slices of the same tensor expose
    slice-parallelism that would be serialized if codelets took raw
    tensor operands — the SSA dep edge still flows through the parent
    tensor, but the analyses see the disjointness.
  }];
  let arguments = (ins
    SdeAccessModeAttr:$mode,
    AnyTensor:$source,
    Variadic<Index>:$offsets,
    Variadic<Index>:$sizes
  );
  let results = (outs SdeTokenType:$token);
  let assemblyFormat = [{
    $mode $source (`[` $offsets^ `]`)? (`size` `[` $sizes^ `]`)?
    `:` type($source) `->` type($token) attr-dict
  }];
}
```

### 4.3 `sde.cu_codelet` — dataflow-isolated compute unit

```tablegen
def SdeCuCodeletOp : ArtsSde_Op<"cu_codelet", [
      IsolatedFromAbove,
      SingleBlock,
      RecursiveMemoryEffects
    ]> {
  let summary = "Dataflow codelet: all I/O through tokens + yield, no captures.";
  let description = [{
    A compute unit whose only communication with the enclosing scope is
    through `sde.mu_token` operands and yielded tensor results. The
    name follows OCR's "codelet" — a purely asynchronous, pure-dataflow
    unit of work whose inputs and outputs are entirely explicit.

    Operand shape: every operand is a `!sde.token<tensor<...>>`. The
    body's block arguments are typed as the token's `slice_type` — the
    op implicitly unpacks tokens at block entry so the body sees
    tensor values of the addressed region.

    Result shape: one SSA result per `write` or `readwrite` token, of
    the same type as the source parent tensor. The op implicitly
    reconstructs the parent tensor with the slice replaced by the
    yielded value — destination-passing style, as in linalg-on-tensors.

    `IsolatedFromAbove` guarantees the body references no SSA value
    from outside its block arguments. Any external memref the source
    task read becomes a `<read>` token; any external memref it wrote
    becomes a `<write>` or `<readwrite>` token. Effects observable to
    the surrounding program flow through `sde.yield`.

    Differences from `sde.cu_task`:
    - No implicit captures of enclosing SSA values.
    - No hidden side effects on shared memrefs; observable effects flow
      through `sde.yield` per writable token.
    - Dependencies are SSA-visible via the token operand list: a
      missing dep is a missing operand, caught by the verifier.
    - Slice-granularity parallelism is expressed on the tokens, not
      buried in the body.
  }];

  let arguments = (ins Variadic<SdeTokenType>:$tokens);
  let results   = (outs Variadic<AnyTensor>:$outputs);
  let regions   = (region SizedRegion<1>:$body);

  let assemblyFormat = [{
    `(` ($tokens^ `:` type($tokens))? `)`
    (`->` `(` type($outputs)^ `)`)?
    $body attr-dict
  }];
}
```

Block-argument invariants:

- One block arg per token, typed as the token's `slice_type`.
- At block entry the op unpacks tokens to slice-typed tensors (readable
  slices for `<read>` and `<readwrite>` tokens; zero-initialized slices
  for `<write>` tokens when no prior content is meaningful).
- `sde.yield` yields one tensor per **writable** token, matching the
  token's slice type; the op's result is the reconstructed parent
  tensor.

### 4.4 Token validation rules (verifiers)

The token design turns correctness-of-deps into a static property
checked by the verifier. The rules below are enforced at IR
construction time — invalid programs never reach downstream passes.

On `sde.mu_token`:

- **V1** `source` is an `AnyTensor` (by operand constraint).
- **V2** If `offsets` / `sizes` are specified, their counts equal the
  source tensor's rank.
- **V3** Static `sizes` are non-negative; when `sizes` + `offsets` are
  constant, `offset + size <= source_dim` for each dimension.

On `sde.cu_codelet`:

- **V4** Each operand is `!sde.token<Ti>` for a ranked tensor `Ti`.
- **V5** Body has exactly one block.
- **V6** Block-arg count = operand count; block-arg types match each
  token's `slice_type` 1:1.
- **V7** Result count = count of **writable** tokens (`<write>` +
  `<readwrite>`).
- **V8** `sde.yield`'s operand count = result count; each yielded type
  equals the matching writable token's `slice_type` (destination-
  passing style).
- **V9** Each result's type equals the parent tensor type of the
  corresponding writable token (the op reconstructs the parent tensor
  with the yielded slice inserted).
- **V10** **A `<read>`-mode token cannot have a yielded counterpart.**
  Reading a token does not produce an output. Trying to "yield an
  input token" is the archetype of an invalid codelet — a `<read>`
  token is a pure acquire with no slot in the destination-passing
  results list. Verifier rejects.
- **V11** Tokens sourced from the same tensor within one codelet's
  operand list must not have conflicting modes on overlapping slices
  (static check when slices are constant; skipped otherwise and
  delegated to runtime analysis).
- **V12** Each token is consumed by exactly one codelet (single-use,
  analogous to `arts.db_acquire` / `db_release` pairing). Enforced by
  a dedicated pattern rather than a trait so that static assertions
  can be relaxed for later peephole passes that split/merge tokens.

Body isolation (from `IsolatedFromAbove`):

- **V13** No SSA value from outside the codelet body appears as an
  operand of any op inside the body. This is a free consequence of
  the trait — no extra verifier code needed.

Together these rules eliminate the entire dep-drop bug class. To lose
a dependency a transform must either (a) drop an SSA operand, which
breaks use-def validity, or (b) turn a readwrite token into a read
token, which fails V7/V8/V10, or (c) yield through a read token,
which fails V10 directly.

### 4.5 `sde.mu_data` — declarative SDE data handle

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

## 7. Pipeline role — `cu_task` vs `cu_codelet`

### 7.1 Today's pipeline is loop-centric

Audit of every pass body in `lib/arts/dialect/sde/Transforms/`: the
SDE analysis and transform passes walk `sde.su_iterate` only. None
of them traverse `sde.cu_task` bodies or `sde.mu_dep` ops for
transformation purposes.

| Pass | Walks `cu_task`? | Walks `su_iterate`? |
| --- | --- | --- |
| `ConvertOpenMPToSde` | emits it | emits it |
| `ScopeSelection` | only the enclosing `cu_region` | yes |
| `ScheduleRefinement` | no | yes |
| `ChunkOpt` | no | yes |
| `ReductionStrategy` | no (future: via `cu_reduce`) | yes |
| `RaiseToLinalg` | no | yes |
| `RaiseToTensor` | no | yes (linalg inside) |
| `LoopInterchange` | no | yes |
| `TensorOpt` | no | yes |
| `StructuredSummaries` | no | yes |
| `ElementwiseFusion` | no | yes |
| `DistributionPlanning` | no | yes |
| `IterationSpaceDecomposition` | no | yes |
| `BarrierElimination` | no (reads `mu_dep` side-effect scope only) | yes |

This means a task-centric program like `samples/deps/deps.c` passes
through 13 SDE analysis passes and emerges structurally unchanged.
The only work the current SDE pipeline does on pure task graphs is
`ScopeSelection` annotating the enclosing `cu_region`. Everything
else runs blind to the task structure.

The `cu_task → cu_codelet` raise is the lever that puts tasks
**into the same regime** as loops. Once a task body is in codelet
form, its access pattern is token-visible and its data flow is
SSA-visible — which is exactly the shape the existing
`su_iterate`-centric passes already know how to analyze.

### 7.2 What belongs on each side of the raise

**Stays on `cu_task` (memref fallback) side, before the raise:**

| Concern | Why | Pass |
| --- | --- | --- |
| OMP → SDE conversion | Task shape is the natural IR for `omp.task` | `ConvertOpenMPToSde` |
| Region-scope annotation | Looks at `cu_region`, not task bodies | `ScopeSelection` |
| Schedule / chunk / reduction strategy on loops | Pure `su_iterate` concerns | `ScheduleRefinement`, `ChunkOpt`, `ReductionStrategy` |
| Loop raise and tensor transforms on loops | `su_iterate` path — already covered | `RaiseToLinalg`, `RaiseToTensor`, `LoopInterchange`, `TensorOpt`, `StructuredSummaries`, `ElementwiseFusion` |

**Moves to `cu_codelet` (tokenized tensor) side, after the raise:**

| Concern | Why tokens help | Pass (today → extended) |
| --- | --- | --- |
| Distribution of task work | Token slices are the distribution unit: codelets with disjoint slices of the same tensor are independent | `DistributionPlanning` extended to walk codelets |
| Dependency graph construction | SSA use-def edge through tokens is the dep graph; nothing to reconstruct from addresses | `ConvertSdeToArts` lowering, future task-graph analyses |
| Access-pattern summarization | Codelet operand modes + slice shapes already state the contract | `StructuredSummaries` extended |
| Codelet fusion | Elementwise codelets forming a use-def chain are fusable | `ElementwiseFusion` extended |
| Bufferization at the SDE→ARTS boundary | Tokens map 1:1 to `arts.db_acquire`; one-shot bufferization fits naturally | New boundary step inside `ConvertSdeToArts` |

**Runs unchanged (loop path stays exactly as today):**

- Every `su_iterate`-centric pass. The raise targets `cu_task`
  only; `su_iterate` bodies and their linalg carriers are not
  touched. There is no regression risk on the loop-centric samples.

### 7.3 Pass-by-pass adaptation

| Pass | Today | After this RFC |
| --- | --- | --- |
| `RaiseToLinalg` | Walks `su_iterate` bodies, emits `linalg.generic(memref)` when the pattern matches. | Walks `su_iterate` bodies, emits `linalg.generic(tensor)` directly — no memref middle step. |
| `RaiseToTensor` | Rewrites emitted `linalg.generic(memref)` → `linalg.generic(tensor)`. | Becomes a narrow cleanup for cases that escaped the main raise. Retires when v3 lands. |
| `ScopeSelection`, `ScheduleRefinement`, `ChunkOpt`, `ReductionStrategy` | Annotate SDE ops (loops + region scopes). | Unchanged. Run before the raise, so SDE ops still carry memref form at annotation time. |
| `LoopInterchange`, `TensorOpt`, `StructuredSummaries`, `ElementwiseFusion`, `IterationSpaceDecomposition` | Walk `su_iterate` in mixed memref/tensor. | Unchanged on loops. Optionally extended to walk `cu_codelet` bodies as a follow-up. |
| `DistributionPlanning` | Walks `su_iterate` only — chooses distribution kind per loop. | Extended to also walk `cu_codelet` — token slices become distribution units for task graphs. |
| `BarrierElimination` | Inspects `mu_dep` / `su_barrier` on loops. | `mu_dep` count drops for raised memrefs; tensor SSA makes the dep graph directly visible. Unchanged on loops. |
| `ConvertSdeToArts` | Memref path via `mu_dep`. | Adds codelet branch (token operands → `db_acquire`, tensor results → `db_alloc` + `db_acquire`). Fallback memref branch unchanged. |

### 7.4 Comparative trace — `parallel_for` vs `deps`

Two reference samples, two very different paths:

**`samples/parallel_for/loops/parallel_for.c` (loop-centric).** After
`ConvertOpenMPToSde`: `sde.cu_region <parallel> { sde.su_iterate ... }`
with `schedule(<static>, %c4)`. Every subsequent pass fires:
`ScopeSelection` annotates scope, `ScheduleRefinement` / `ChunkOpt` /
`ReductionStrategy` annotate the loop, `RaiseToLinalg` emits a
`linalg.generic` carrier, `RaiseToTensor` converts it to tensor form,
and the remaining passes reshape the loop body. **Nothing is gained
by the raise here** — there are no `cu_task`s to convert, and
`su_iterate` already has the full tensor path. The pass is a no-op
on this sample.

**`samples/deps/deps.c` (task-centric).** After `ConvertOpenMPToSde`:
`sde.cu_region <parallel> { sde.cu_region <single> { cu_task ...;
mu_dep ...; cu_task ... } }` — purely tasks + deps, no
`su_iterate`. `ScopeSelection` annotates the region; **every other
SDE pass is a no-op**. The program reaches `ConvertSdeToArts`
unprocessed. **The raise is what unlocks this sample**: after it
runs, the three tasks become `cu_codelet` ops with explicit
`!sde.token<tensor<i32>>` operands, the dep graph is SSA-visible,
and `ConvertSdeToArts` has concrete token-based lowering instead
of address-tracked `mu_dep` reconstruction.

The raise is **neutral on loops and transformative on tasks** —
exactly the leverage we want. That is the one-sentence answer to
"what belongs on which side of the raise".

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

See `samples/deps/pipelines/boundaries/01_omp_to_sde/after.mlir`
for the current form. After `RaiseMemrefToTensor` (expected shape):

```mlir
%a0 = arts_sde.mu_data : tensor<i32>  {init = 0 : i32}
%b0 = arts_sde.mu_data : tensor<i32>  {init = 0 : i32}

%a_final, %b_final = arts_sde.cu_region <parallel>
    (%a_in = %a0, %b_in = %b0 : tensor<i32>, tensor<i32>)
    -> (tensor<i32>, tensor<i32>) {
  %a_s, %b_s = arts_sde.cu_region <single> scope(<local>)
      (%as = %a_in, %bs = %b_in : tensor<i32>, tensor<i32>)
      -> (tensor<i32>, tensor<i32>) {

    // Task 1: produces `a`. One <write> token -> one result.
    %tok_a_w = arts_sde.mu_token <write> %as
             : tensor<i32> -> !arts_sde.token<tensor<i32>>
    %a_t1 = arts_sde.cu_codelet(%tok_a_w
                                : !arts_sde.token<tensor<i32>>)
            -> (tensor<i32>) {
    ^bb0(%a_init : tensor<i32>):
      %r = func.call @rand() : () -> i32
      %v = arith.remsi %r, %c100_i32 : i32
      %t = tensor.from_elements %v : tensor<i32>
      arts_sde.yield %t : tensor<i32>
    }

    // Task 2: reads `a`, produces `b`. One <read> token + one <write> token.
    // The <read> token has NO yielded counterpart (V10).
    %tok_a_r = arts_sde.mu_token <read>  %a_t1
             : tensor<i32> -> !arts_sde.token<tensor<i32>>
    %tok_b_w = arts_sde.mu_token <write> %bs
             : tensor<i32> -> !arts_sde.token<tensor<i32>>
    %b_t2 = arts_sde.cu_codelet(%tok_a_r, %tok_b_w
                                : !arts_sde.token<tensor<i32>>,
                                  !arts_sde.token<tensor<i32>>)
            -> (tensor<i32>) {
    ^bb0(%a_view : tensor<i32>, %b_init : tensor<i32>):
      %va = tensor.extract %a_view[] : tensor<i32>
      %nb = arith.addi %va, %c5_i32 : i32
      %t  = tensor.from_elements %nb : tensor<i32>
      arts_sde.yield %t : tensor<i32>
      // yields one tensor == one <write> token; V10 holds (we did not
      // yield the %a_view input).
    }

    // Task 3: read-only. Zero writable tokens -> zero codelet results.
    %tok_b_r = arts_sde.mu_token <read> %b_t2
             : tensor<i32> -> !arts_sde.token<tensor<i32>>
    arts_sde.cu_codelet(%tok_b_r : !arts_sde.token<tensor<i32>>) -> () {
    ^bb0(%b_view : tensor<i32>):
      %vb = tensor.extract %b_view[] : tensor<i32>
      // … printf …
      arts_sde.yield
    }

    arts_sde.yield %a_t1, %b_t2 : tensor<i32>, tensor<i32>
  }
  arts_sde.yield %a_s, %b_s : tensor<i32>, tensor<i32>
}

// Boundary materialization: tensor -> memref for post-parallel scalar reads.
%a_final_scalar = tensor.extract %a_final[] : tensor<i32>
%b_final_scalar = tensor.extract %b_final[] : tensor<i32>
memref.store %a_final_scalar, %alloca_0[%c0] : memref<1xi32>
memref.store %b_final_scalar, %alloca[%c0]   : memref<1xi32>
```

Every task is `cu_codelet`. Task 2's dep on Task 1 is the SSA operand
chain `%a_t1 → %tok_a_r → cu_codelet`. No `mu_dep` remains. Task 1 no
longer has a hidden write — its output is an SSA result the verifier
tracks. Dropping any dep requires detaching a use-def edge, which the
verifier refuses. Trying to yield the `%a_view` input of Task 2 would
fail V10 at verification time.

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
