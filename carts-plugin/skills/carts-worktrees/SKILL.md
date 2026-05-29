---
name: carts-worktrees
description: Use when working on multiple CARTS/ARTS changes in parallel, isolating a risky compiler/runtime change, or running concurrent builds/benchmarks without clobbering the main checkout. Covers the carts-wt tool and the shared-LLVM/Polygeist worktree model.
user-invocable: true
allowed-tools: Bash, Read, Grep, Glob
argument-hint: [new|build|run|list|doctor|rm <name>]
---

# CARTS worktrees (parallel dev, shared LLVM/Polygeist)

Run several carts/arts changes side by side, each with its **own isolated
build**, while **sharing** the heavy prebuilt LLVM + Polygeist (≈11 GB) and the
dekk conda toolchain with the main checkout. Use this instead of editing the
main tree when you want to: build/test two fixes at once, keep a risky change
isolated, or run a long benchmark while continuing to edit.

The tool is **`tools/carts-wt`** in the carts-cgo harness
(`<carts-cgo>/tools/carts-wt`). It encodes the model below; prefer it over
hand-rolling `git worktree`.

## Quick start
```
carts-wt new   <name> [base-ref] [--build]   # create (default base = main carts HEAD)
carts-wt build <name> [--arts]               # build carts compiler (or arts runtime) in it
carts-wt run   <name> -- benchmarks run polybench/3mm --size megalarge --nodes 2 ...
carts-wt list                                # name / carts HEAD / arts HEAD / path
carts-wt doctor <name>                       # verify isolation + shared deps
carts-wt rm    <name> [--force]              # remove worktree + branch + symlinks
```
Worktrees live under `<carts-cgo>/worktrees/<name>` (gitignored). Each is a
real carts checkout → `dekk carts ...` run from inside it resolves to **that**
worktree (dekk walks up to the `.git` marker).

## The model (why a naive `git worktree` fails — read before improvising)
The buildable unit is the **carts repo** (`submodules/carts`), which owns
`lib/carts` and the `external/arts` submodule. Four traps the tool handles:

1. **Worktree the carts repo, not the carts-cgo superproject.** In carts-cgo,
   `submodules/carts` is an *untracked sibling clone* (only `submodules/.gitkeep`
   is tracked; the pin lives in `pin/commits.lock`). A superproject worktree has
   an **empty `submodules/`** — useless. Always `git worktree add` on
   `submodules/carts`.

2. **Local-only WIP submodule commits.** `external/arts` (and
   `external/carts-benchmarks`) are often pinned to commits that exist **only in
   the local main checkout**, never pushed. `git submodule update --init` clones
   from the remote and **cannot** fetch them (it silently lands on an older
   commit). The tool fetches the exact pin from the LOCAL main submodule and
   checks it out. Verify with `carts-wt doctor` / `git -C <wt>/external/arts rev-parse HEAD`.

3. **Share the heavy deps, build only carts+arts.** Build output resolves to
   `CARTS_HOME/{build,.install}/<subproject>`, where `CARTS_HOME` defaults to
   the checkout root. The tool **symlinks** the stable, expensive pieces from
   main — `build/llvm-project`, `build/polygeist`, `.install/llvm`,
   `.install/polygeist`, `external/Polygeist` (source, for cmake includes), and
   `.dekk/env` (the 1.8 GB conda toolchain) — and leaves `build/carts`,
   `build/arts`, `.install/{carts,arts}` as **real per-worktree dirs**. So
   `dekk carts build` / `--arts` rebuild only carts/arts (minutes), never LLVM
   (hours), and **never clobber main** (carts/arts are never symlinks — `doctor`
   asserts this). Net worktree disk cost ≈ 13 MB + the carts/arts build.

4. **Never run `dekk carts build --llvm`/`--polygeist` in a worktree.** Those
   targets would try to build into the symlinked shared dirs → clobber main.
   Only the main checkout builds LLVM/Polygeist. If main lacks them, `carts-wt`
   refuses with a clear message — build main first.

## Verify before trusting a worktree
`carts-wt doctor <name>` checks: it's a carts worktree, arts is checked out,
every shared dep is a symlink that resolves, `build/{carts,arts}` are **local**
(not symlinks), and `carts-compile` is built. Treat a FAIL as a setup bug, not a
reason to edit the main tree.

## Cluster runs
Worktrees sit on the shared filesystem, so slurm benchmark runs work from them:
`carts-wt run <name> -- benchmarks run polybench/<b> --size megalarge --nodes 2
--threads 64 --arts --compile-args "--distributed-db" --rdma --results-dir <dir>`.
(See `carts-multinode-examples` for flags; megalarge + `--distributed-db` are
required for 2-node scaling claims.)

## Cleanup
`carts-wt rm <name>` removes the worktree, deletes its `wt/<name>` branch, and
prunes the registration. It refuses to remove the main checkout.

## Gotchas log (already-paid debugging)
- The Agent tool's `isolation: worktree` makes a **superproject** worktree here
  → empty `submodules/` → cannot build carts. Use `carts-wt`, not Agent worktree
  isolation, for carts/arts work.
- A worktree's `git status` shows `external/Polygeist` and the symlinked dirs as
  changes — that's expected (carts `.gitignore` already ignores
  `build/`, `.install`, `.dekk/`); never commit them.
- Two concurrent builds only contend for CPU, not correctness. Lower
  `CARTS_BUILD_JOBS` if a machine thrashes.
