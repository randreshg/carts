---
name: carts-review-conventions
description: Check CARTS coding conventions in changed or specified files. Detects hardcoded attribute strings, direct graph access, naming violations, analysis invalidation issues, and utility duplication. Use before committing, during PR review, or for codebase audits.
user-invocable: true
allowed-tools: Read, Grep, Glob, Bash, Agent
argument-hint: [<file-or-dir> | --changed | --all]
---

# CARTS Convention Review

## Purpose

Automated convention checking for CARTS code. Catches violations that cause
silent bugs (hardcoded strings, stale analysis) or technical debt (duplicate
functions, wrong file placement).

## Convention Checklist

### 1. Attribute Names (CRITICAL — silent miscompile risk)

**Rule**: NEVER hardcode attribute name strings. Use `AttrNames::Operation::*`
from `OperationAttributes.h` and `AttrNames::Operation::Stencil::*` from
`StencilAttributes.h`.

**Detection**:
```bash
# Find potential hardcoded attribute strings in a file
grep -n '"arts\.' <file> | grep -v 'AttrNames\|DEBUG\|ARTS_DEBUG\|arts_debug'
```

**Known violations** (from audit):
- `BlockLoopStripMiningSupport.cpp:18` — `"arts.block_loop_strip_mining.generated"`
- `RaiseToLinalg.cpp:240-295` — `"parallel"`, `"reduction"`, `"stencil"`
- `EdtLowering.cpp:123` — hardcoded op names
- `MetadataAttacher/Registry/Manager` — `ArtsMetadata::IdAttrName` vs `AttrNames::Operation::ArtsId`

### 2. Analysis Access (CRITICAL — thread safety)

**Rule**: NEVER access graphs directly. Use `AM->getDbAnalysis()`,
`AM->getEdtAnalysis()`, `AM->getLoopAnalysis()`.

**Detection**:
```bash
# Find direct graph access
grep -n 'getDbGraph\|getEdtGraph\|\.getGraph()' <file>
# Find unserialized invalidation
grep -n 'AM->invalidate\|\.invalidate(' <file>
```

### 3. Utility Duplication (MEDIUM — maintenance burden)

**Rule**: Before adding any static helper, check if it exists in shared utils.
Use `/check-utils <function-name>`.

**Detection**:
```bash
# Find static functions that might duplicate shared utils
grep -n '^static ' <file> | grep -v 'const\|Statistic'
```

### 4. Naming Conventions (LOW — readability)

**Rule**: DB passes use `Db` prefix. EDT passes use `Edt` prefix. LLVM style:
2-space indent, CamelCase types, camelCase variables.

### 5. File Placement (MEDIUM — architecture integrity)

**Rule**: Files must be in the correct dialect directory:
- Core transforms: `lib/arts/dialect/core/Transforms/`
- ArtsToRt conversion: `lib/arts/dialect/core/Conversion/ArtsToRt/`
- SDE transforms: `lib/arts/dialect/sde/Transforms/`
- Shared utils: `lib/arts/utils/`

### 6. Pass Registration (MEDIUM — pipeline integrity)

**Rule**: Every pass must be registered in `tools/compile/Compile.cpp` at the
correct pipeline stage. Stage ordering is load-bearing.

## Quick Scan Commands

```bash
# Hardcoded attribute strings across codebase
grep -rn '"arts\.' lib/arts/ --include='*.cpp' | grep -v AttrNames | grep -v DEBUG | head -20

# Direct graph access violations
grep -rn 'getDbGraph\|getEdtGraph' lib/arts/ --include='*.cpp' | head -10

# Static functions in pass files (potential duplicates)
grep -rn '^static.*(' lib/arts/dialect/*/Transforms/*.cpp | wc -l

# Analysis invalidation calls
grep -rn '\.invalidate(' lib/arts/ --include='*.cpp' | head -10

# Thread-unsafe global/static mutable state
grep -rn 'static.*=' lib/arts/dialect/ --include='*.cpp' | grep -v 'const\|Statistic\|StringLiteral' | head -10
```

## Instructions

When asked to review conventions:

1. Determine scope: specific file, changed files (`git diff --name-only`), or all
2. Run checks in parallel:
   - Hardcoded attribute strings (grep for `"arts\.` without AttrNames)
   - Direct graph access (grep for `getDbGraph`/`getEdtGraph`)
   - Analysis invalidation (grep for `.invalidate(`)
   - Static function duplication (compare against utility catalog)
3. For each violation, report:
   - File:line
   - Convention violated
   - Correct pattern to use
   - Severity (CRITICAL / MEDIUM / LOW)
4. Suggest fixes with code examples
5. Run `/check-utils` for any new static functions found
