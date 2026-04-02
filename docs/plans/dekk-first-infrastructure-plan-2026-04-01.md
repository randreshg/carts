# CARTS Dekk-First Conda Migration Plan

Date: 2026-04-01

## Decision

CARTS should run on a dekk-first, conda-backed infrastructure model with no
checked-in bootstrap wrappers, no Poetry-managed CLI environment, and no
project-local reimplementation of dekk project features.

## End State

Project lifecycle:

```bash
pip install dekk
dekk carts setup
dekk carts install
dekk carts agents generate
dekk carts worktree list
```

Daily usage:

```bash
carts build
carts compile foo.c -O3
carts test
carts pipeline
```

## Ownership

- `dekk` owns environment setup, install orchestration, wrapper installation,
  project agents, and worktree support
- `environment.yml` is the Python environment source of truth
- `.dekk.toml` is the project lifecycle source of truth
- `.agents/` is the committed agent source of truth
- `tools/carts_cli.py` owns compiler-domain commands only

## What Must Not Exist

- checked-in bootstrap wrappers such as `tools/carts`
- repo-local bootstrap venv flows
- Poetry-managed CLI packaging for normal development
- custom `carts agents ...` behavior that duplicates dekk project routing
- lit tests that bypass the installed CLI with `python tools/carts_cli.py`

## Phases

### Phase 1: Conda + dekk as the only bootstrap path

Status: completed in repo

- `.dekk.toml` uses `type = "conda"`
- `environment.yml` defines the bootstrap environment
- `dekk carts install` delegates into `tools/install_driver.py`
- Poetry files and checked-in bootstrap scripts were removed

### Phase 2: Move agent source-of-truth to dekk defaults

Status: completed in repo

- move committed agent source from `.carts/` to `.agents/`
- set `[agents].source = ".agents"` in `.dekk.toml`
- remove the custom `agents` sub-app from `tools/carts_cli.py`
- standardize docs on `dekk carts agents ...`

Rationale:

- dekk 1.6.x does not honor a custom `[agents].source` for built-in project
  routing
- using `.agents/` removes that upstream blocker and lets CARTS use dekk’s
  built-in project `agents` flow directly

### Phase 3: Remove direct Python test bypasses

Status: completed in repo

- add `%carts` lit substitution pointing at `.install/bin/carts`
- rewrite contract tests to call `%carts compile ...`
- keep `%carts-compile` as the installed low-level compiler substrate

### Phase 4: Align Docker with the same bootstrap model

Status: in progress in repo

- install conda and dekk in the Docker base image
- bootstrap cloned workspaces with `dekk carts install --no-interactive`
- prefer the installed wrapper in container PATH

Remaining work:

- validate the full Docker build path end-to-end

## Validation Checklist

1. `dekk carts --help`
2. `dekk carts setup --help`
3. `dekk carts install --help`
4. `dekk carts agents status`
5. `dekk carts agents list`
6. `carts --help`
7. `carts test`
8. `carts lit tests/contracts/...`

## Acceptance Criteria

The migration is complete when all of the following are true:

1. A fresh clone can bootstrap through `pip install dekk && dekk carts install`.
2. Agent generation works through `dekk carts agents ...` with `.agents/` as the
   committed source of truth.
3. No tracked docs tell users to use Poetry, `tools/carts`, or direct
   `python tools/carts_cli.py` entrypoints.
4. Lit and contract tests use installed commands, not source-tree Python
   bypasses.
5. Docker uses the same dekk + conda bootstrap model as local development.
