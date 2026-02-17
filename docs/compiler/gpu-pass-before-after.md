# GPU Passes: Before/After Contracts

This document captures the expected IR deltas for GPU-related passes and
transformations in the CARTS pipeline.

## Contract: EDT Dependencies Must Be DB-Backed

`arts.edt` dependency operands are expected to trace to `arts.db.acquire`
(or dependency acquire forms that trace to DB acquisition). GPU lowering must
not introduce raw memref dependencies.

## `ConvertOpenMPToArts` (`--gpu` enabled)

Before:
```mlir
scf.parallel (%i) = (%lb) to (%ub) step (%step) {
  ...
}
```

After:
```mlir
arts.for(%lb) to(%ub) step(%step) {{
^bb0(%i: index):
  ...
}} {gpu_target = #arts.gpu_target<auto>}
```

## `GpuEligibilityAnalysis`

Before:
```mlir
arts.for(%lb) to(%ub) step(%step) {{ ... }}
```

After:
```mlir
arts.for(%lb) to(%ub) step(%step) {{ ... }}
  {gpu_target = #arts.gpu_target<auto>, gpu_profitability = #arts.gpu_profitability<...>}
```

## `GpuForLowering`

Before:
```mlir
arts.for(%lb) to(%ub) step(%step) {{ ... }}
```

After (DB-backed deps only):
```mlir
%g = arts.gpu_edt route(%route) launch(<gx, gy, gz, bx, by, bz>) (%dep0, %dep1) : ... {
  %tid = gpu.thread_id x
  %bid = gpu.block_id x
  %bdim = gpu.block_dim x
  ...
}
```

Fallback:
- If any loop dependency is not DB-backed, the pass skips GPU lowering and
  leaves `arts.for` unchanged.

## `GpuEdtLowering`

Before:
```mlir
%g = arts.gpu_edt route(%route) launch(<...>) (%deps...) : ... {
  ...
}
```

After:
```mlir
arts.edt <task> <intranode> route(%route) (%deps...) : ... {
  ...
} {gpu_launch_config = #arts.gpu_config<...>}
```

## `EdtLowering` (GPU path)

Before:
```mlir
arts.edt <task> <intranode> route(%route) (%deps...) : ... {
  ...
} {gpu_launch_config = #arts.gpu_config<...>}
```

After:
```mlir
%pack = arts.edt_param_pack(...)
%guid = arts.edt_create(%pack : memref<?xi64>) depCount(%n) route(%route)
  {outlined_func = "...", gpu_launch_config = #arts.gpu_config<...>}
arts.rec_dep %guid(...)
```

## `GpuCodegen`

Before:
```mlir
... // module may still contain arts.gpu_edt
```

After:
- No IR rewrite.
- Verifies that `arts.gpu_edt` does not remain at codegen boundary.

## `ConvertArtsToLLVM` GPU Lowering

Before:
```mlir
%guid = arts.edt_create(...) {gpu_launch_config = #arts.gpu_config<...>}
gpu.thread_id x
gpu.block_id x
gpu.block_dim x
arts.lc_sync
```

After:
```mlir
%dim3_grid  = llvm.insertvalue ...
%dim3_block = llvm.insertvalue ...
%guid = llvm.call @artsEdtCreateGpu(...)
%tid  = nvvm.read.ptx.sreg.tid.x
%bid  = nvvm.read.ptx.sreg.ctaid.x
%bdim = nvvm.read.ptx.sreg.ntid.x
llvm.call @artsLCSync(...)
```
