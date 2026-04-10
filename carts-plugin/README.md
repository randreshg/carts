# CARTS Plugin for Claude Code

Hybrid plugin for the CARTS (Compiler for Asynchronous Runtime Systems) MLIR
compiler. Combines reasoning-heavy skills with structured MCP tools and
automation hooks.

## Architecture

```
carts-plugin/
├── .claude-plugin/plugin.json    # Plugin manifest
├── .mcp.json                     # MCP server configuration
├── skills/                       # 21 skills (reasoning layer)
│   ├── build/                    # /carts:build
│   ├── test/                     # /carts:test
│   ├── carts/                    # /carts:carts (CLI reference)
│   ├── debug/                    # /carts:debug (master debugger)
│   ├── pass-dev/                 # /carts:pass-dev
│   ├── benchmark/                # /carts:benchmark
│   ├── miscompile-triage/        # /carts:miscompile-triage
│   ├── runtime-triage/           # /carts:runtime-triage
│   ├── distributed-triage/       # /carts:distributed-triage
│   ├── analysis-triage/          # /carts:analysis-triage
│   ├── benchmark-triage/         # /carts:benchmark-triage
│   ├── reproducer/               # /carts:reproducer
│   ├── runtime-first/            # /carts:runtime-first
│   ├── check-utils/              # /carts:check-utils
│   ├── refactor-utils/           # /carts:refactor-utils
│   ├── contract-refresh/         # /carts:contract-refresh
│   ├── create-test/              # /carts:create-test
│   ├── stage-diff/               # /carts:stage-diff
│   ├── dialect-trace/            # /carts:dialect-trace
│   ├── heuristic-explain/        # /carts:heuristic-explain
│   └── review-conventions/       # /carts:review-conventions
├── workflows/                    # Python workflow engines (16 modules)
├── hooks/hooks.json              # Automation hooks
│   PostToolUse(Write|Edit):        auto-format C++/TableGen
│   SessionStart:                   validate environment
│   PreToolUse(Bash):               block raw make/ninja
├── mcp/                          # MCP server (structured data layer)
│   ├── carts_server.py           # 5 tools: compile, pipeline, diagnose, doctor, test
│   └── requirements.txt
└── bin/
    └── carts-env-check           # Environment validation script
```

## Design Principles

**Skills for reasoning, MCP for data, hooks for automation.**

- **Skills** (21): Expert playbooks Claude follows with full model reasoning.
  Triage, debugging, pass development — anything requiring judgment and
  multi-step investigation stays as a skill.

- **MCP tools** (5): Thin wrappers around `dekk carts` CLI commands that return
  structured JSON. Compilation output gets parsed into `{ir, errors, warnings}`
  instead of raw text. Diagnostics return machine-readable JSON.

- **Hooks** (4): Automated actions on lifecycle events. Format C++ on save,
  validate environment on session start, block raw build commands.

## Installation

### Local development (from project root)

```bash
# Install the plugin for this project
claude /plugin install --plugin-dir ./carts-plugin --scope project
```

### For all projects

```bash
claude /plugin install --plugin-dir /path/to/carts-plugin --scope user
```

## MCP Server Setup

The MCP server requires the `mcp` Python package:

```bash
pip install -r carts-plugin/mcp/requirements.txt
```

## Skills Reference

### Core Workflow
| Skill | Description |
|-------|-------------|
| `/carts:build` | Build compiler, runtime, Polygeist, LLVM |
| `/carts:test` | Run test suite, lit tests, single files |
| `/carts:carts` | CLI reference and command guide |
| `/carts:pass-dev` | Guide for creating/modifying compiler passes |
| `/carts:benchmark` | Run and manage benchmarks |

### Triage & Debugging
| Skill | Description |
|-------|-------------|
| `/carts:debug` | Master debugger — classifies then delegates |
| `/carts:miscompile-triage` | Wrong-code bugs, semantic drift |
| `/carts:runtime-triage` | Hangs, crashes, deadlocks |
| `/carts:distributed-triage` | Multi-node failures |
| `/carts:analysis-triage` | Stale analysis, invalidation bugs |
| `/carts:benchmark-triage` | Benchmark-specific failures |
| `/carts:runtime-first` | Understand runtime first, fix compiler second |
| `/carts:reproducer` | Reduce failing cases to minimal tests |

### Code Quality
| Skill | Description |
|-------|-------------|
| `/carts:check-utils` | Verify function doesn't duplicate shared utils |
| `/carts:refactor-utils` | Extract misplaced helpers to shared locations |
| `/carts:review-conventions` | Check coding conventions in changed files |

### Pipeline Analysis
| Skill | Description |
|-------|-------------|
| `/carts:stage-diff` | Compare IR between pipeline stages |
| `/carts:dialect-trace` | Trace op lifecycle across 3 dialects |
| `/carts:heuristic-explain` | Explain partitioning/distribution decisions |

### Testing
| Skill | Description |
|-------|-------------|
| `/carts:create-test` | Scaffold new contract/integration tests |
| `/carts:contract-refresh` | Refresh stale test fixtures after pass changes |

## MCP Tools Reference

| Tool | Description |
|------|-------------|
| `carts_compile` | Compile file, return structured `{ir, errors, warnings}` |
| `carts_pipeline` | List 18 pipeline stages (source of truth) |
| `carts_diagnose` | Run diagnostics, return JSON decisions |
| `carts_doctor` | Validate environment health |
| `carts_test` | Run tests, return structured `{passed, failed, failures}` |

## Hooks Reference

| Event | Action |
|-------|--------|
| `PostToolUse(Write\|Edit)` on `*.cpp\|*.h\|*.td` | Auto-run `dekk carts format` |
| `SessionStart` | Run `dekk carts doctor --quiet` |
| `PreToolUse(Bash)` matching `make\|ninja\|cmake` | Block with guidance message |
