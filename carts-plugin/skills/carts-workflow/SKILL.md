---
name: carts-workflow
description: Use for general CARTS development tasks, including building, worktrees, and committing.
---

# CARTS Workflow

- **CLI Usage**: Use `dekk carts ...` inside `submodules/carts` for CARTS compiler, runtime, lit, and benchmark commands. Do not use raw ninja/cmake directly unless debugging.
- **Worktrees**: Use `carts-wt` (or `dekk carts worktrees`) for managing worktrees when isolating risky changes.
- **Committing**: Ensure all SDE/ARTS layer boundaries and invariants are respected before committing.
