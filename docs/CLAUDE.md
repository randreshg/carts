# docs/ — Architecture and Developer Documentation

## Contents

```
getting-started.md                  Setup and first compilation
developer-guide.md                  Supported constructs and memory layouts
contributing.md                     Coding style, testing, commit guidelines
compiler/
  pipeline.md                       Pass ordering and stage ownership
  arts-dep-transforms.md            Dependence transform documentation
  arts-kernel-transforms.md         Kernel transform documentation
  epoch-finish-continuation-rfc.md  Epoch continuation RFC
  gpu-plan.md                       GPU support plan
heuristics/
  partitioning.md                   DB partitioning heuristics (H1.x)
  distribution.md                   EDT distribution strategy
  db_granularity.md                 DB granularity decisions
research/                           Research notes and investigations
```

## Agent Usage

- `AGENTS.md` (root) is the primary agent reference with full pipeline details
- These docs provide deeper coverage of specific subsystems
- `compiler/pipeline.md` is the canonical pipeline ordering reference
- `heuristics/partitioning.md` documents the H1.x decision chain
