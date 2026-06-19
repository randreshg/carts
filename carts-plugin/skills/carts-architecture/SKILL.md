---
name: carts-architecture
description: Use to understand CARTS boundaries, pass pipelines, dialect maps, and runtime execution models.
---

# CARTS Architecture

- **Code is Truth**: Find ground truth in code, not static tables.
- **Pipelines**: For pipeline details and pass order, read `lib/carts/pipeline/Pipeline.cpp`.
- **Layering**: 
  - **SDE** owns real source/layout/tiling transforms and planning. 
  - **ARTS** realizes this as DB/EDT execution graphs and owner routes.
  - **ARTS-RT** is the mechanical runtime ABI layer.
- **Invariants**: Do not recompute committed facts (owner dims, DB grain) downstream. SDE plans, ARTS realizes.
