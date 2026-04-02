# MLIR PassInstrumentation Hook Design for CARTS

**Date**: 2026-04-02
**Investigation**: G-10 — Pass Instrumentation Hook Design
**Scope**: Understanding MLIR's PassInstrumentation API and designing CARTS integration

---

## Executive Summary

MLIR provides a robust PassInstrumentation system that allows plugins to hook into pass execution lifecycle events. CARTS currently uses:
- **`--mlir-timing`** (MLIR built-in): Wall-clock time per pass and analysis
- **`--diagnose`** (CARTS custom): Captures per-stage debug output via `AnalysisManager`

This investigation examines:
1. MLIR's PassInstrumentation API and thread-safety guarantees
2. Existing implementations (PassTiming, IRPrinter, CrashRecovery)
3. Current CARTS instrumentation gaps
4. Proposed CartsPassInstrumentation design with 6 features

---

## 1. MLIR PassInstrumentation API

### 1.1 Core Classes

**`PassInstrumentation` (abstract base class)**
- Located: `external/Polygeist/llvm-project/mlir/include/mlir/Pass/PassInstrumentation.h`
- 8 virtual methods (all with default empty implementations):

```cpp
class PassInstrumentation {
public:
  virtual ~PassInstrumentation() = 0;

  // Pipeline hooks
  virtual void runBeforePipeline(std::optional<OperationName> name,
                                 const PipelineParentInfo &parentInfo);
  virtual void runAfterPipeline(std::optional<OperationName> name,
                                const PipelineParentInfo &parentInfo);

  // Pass hooks
  virtual void runBeforePass(Pass *pass, Operation *op) {}
  virtual void runAfterPass(Pass *pass, Operation *op) {}
  virtual void runAfterPassFailed(Pass *pass, Operation *op) {}

  // Analysis hooks
  virtual void runBeforeAnalysis(StringRef name, TypeID id, Operation *op) {}
  virtual void runAfterAnalysis(StringRef name, TypeID id, Operation *op) {}
};
```

**`PassInstrumentor` (container class)**
- Manages a collection of PassInstrumentation objects
- Multiplexes callbacks to all registered instrumentations
- Used internally by PassManager
- Thread-safe: callback chains use DenseMap of per-thread timers in PassTiming

**`PipelineParentInfo` (context struct)**
```cpp
struct PipelineParentInfo {
  uint64_t parentThreadID;     // Thread ID from llvm::get_threadid()
  Pass *parentPass;             // The pass that spawned this pipeline
};
```

### 1.2 Registration Point

PassManager provides single registration method:
```cpp
void PassManager::addInstrumentation(std::unique_ptr<PassInstrumentation> pi)
```

Multiple instrumentations can be registered — they are invoked in order.

### 1.3 Execution Model

For each pass on each operation:
1. `runBeforePass(pass, op)` — before execution
2. [Pass runs, modifies IR or computes analysis]
3. Either `runAfterPass(pass, op)` (success) OR `runAfterPassFailed(pass, op)` (failure)

For each analysis computation:
1. `runBeforeAnalysis(name, id, op)` — before lazy analysis init
2. [Analysis computes]
3. `runAfterAnalysis(name, id, op)` — after computation

### 1.4 Thread Safety Guarantees

- **Per-thread hooks**: PassTiming uses DenseMap keyed by `uint64_t threadID` from `llvm::get_threadid()`
- **Callback order**: All instrumentations invoked sequentially on same thread as pass execution
- **Parent tracking**: `PipelineParentInfo` allows instrumentations to track parent->child thread relationships
- **No mutex required**: MLIR context disables multithreading in Compile.cpp line 600

---

## 2. Existing PassInstrumentation Implementations

### 2.1 PassTiming (MLIR Built-in)

**File**: `external/Polygeist/llvm-project/mlir/lib/Pass/PassTiming.cpp`

Features:
- Wall-clock timing via `mlir::TimingScope` (thread-aware)
- Measures pass execution time and analysis computation time separately
- Per-thread timer stack tracking
- Parent timer index map for spawned pipelines

Implementation strategy:
```cpp
struct PassTiming : public PassInstrumentation {
  DenseMap<uint64_t, SmallVector<TimingScope, 4>> activeThreadTimers;
  DenseMap<PipelineParentInfo, unsigned> parentTimerIndices;

  void runBeforePass(Pass *pass, Operation *) override {
    auto &activeTimers = activeThreadTimers[llvm::get_threadid()];
    activeTimers.push_back(parentScope.nest(...));
  }

  void runAfterPass(Pass *pass, Operation *) override {
    auto &activeTimers = activeThreadTimers[llvm::get_threadid()];
    activeTimers.pop_back();
  }
};
```

Limitations:
- Does NOT capture per-pass IR modifications
- Does NOT collect pass-level statistics (llvm::Statistic)
- Does NOT integrate with CARTS --diagnose system

### 2.2 IRPrinterInstrumentation (MLIR Built-in)

**File**: `external/Polygeist/llvm-project/mlir/lib/Pass/IRPrinting.cpp`

Features:
- Dump IR before/after pass execution
- Change detection using OperationFingerPrint
- Configurable print scope (module vs. local)

Implementation strategy:
```cpp
class IRPrinterInstrumentation : public PassInstrumentation {
  DenseMap<Pass *, OperationFingerPrint> beforePassFingerPrints;

  void runBeforePass(Pass *pass, Operation *op) override {
    if (config->shouldPrintAfterOnlyOnChange())
      beforePassFingerPrints.try_emplace(pass, op);
    config->printBeforeIfEnabled(pass, op, ...);
  }

  void runAfterPass(Pass *pass, Operation *op) override {
    if (config->shouldPrintAfterOnlyOnChange()) {
      bool changed = beforePassFingerPrints[pass] != OperationFingerPrint(op);
      if (!changed) return;
    }
    config->printAfterIfEnabled(pass, op, ...);
  }
};
```

Limitations:
- Expensive fingerprinting on every pass (no lazy evaluation)
- Only detects structural changes, not semantic ones

### 2.3 CrashRecoveryInstrumentation (MLIR Built-in)

**File**: `external/Polygeist/llvm-project/mlir/lib/Pass/PassCrashRecovery.cpp`

Features:
- Tracks currently executing pass for crash reproduction
- Generates minimal reproducer IR + pipeline script on segfault
- Global state managed via ManagedStatic mutex

**Not recommended for CARTS:** CARTS is stable (no crash instrumentation needed).

---

## 3. Current CARTS Instrumentation State

### 3.1 Compile.cpp Integration

**Location**: `tools/compile/Compile.cpp:572-578`

```cpp
static LogicalResult configurePassManager(PassManager &pm) {
  pm.enableVerifier(true);
  if (failed(applyPassManagerCLOptions(pm)))
    return failure();
  applyDefaultTimingPassManagerCLOptions(pm);  // Enables --mlir-timing
  return success();
}
```

**Current setup:**
- Calls `applyDefaultTimingPassManagerCLOptions(pm)` which registers PassTiming if `--mlir-timing` flag is present
- No custom instrumentation registered

### 3.2 Existing --diagnose System

**Location**: `tools/compile/Compile.cpp:1360-1395`

```cpp
if (Diagnose) {
  hooks.afterStep = [](StageId stage, LogicalResult result) {
    ARTS_INFO("Pipeline " << stageName(stage)
                          << (succeeded(result) ? " completed" : " FAILED"));
  };
  hooksPtr = &hooks;
}

// Later:
if (Diagnose && AM) {
  bool includeAnalysis = shouldExportDetailedDiagnose(*resolvedStopAt) &&
                         AM->hasCapturedDiagnostics();
  AM->exportToJson(outputFile, includeAnalysis);
}
```

**Limitations:**
- Hooks are PipelineHooks (custom struct), NOT PassInstrumentation
- Per-STAGE diagnostics only, not per-PASS
- Requires explicit `captureDiagnostics()` call in AnalysisManager
- No timing data in JSON output

### 3.3 AnalysisManager Diagnostic Interface

**Location**: `include/arts/analysis/AnalysisManager.h:78-85`

```cpp
public:
  void captureDiagnostics();
  void exportToJson(llvm::raw_ostream &os, bool includeAnalysis = false);
  bool hasCapturedDiagnostics() const {
    return cachedDiagnosticJson.has_value();
  }
```

Current behavior:
- `captureDiagnostics()` captures graph structure and per-loop metadata
- `exportToJson()` serializes captured data to JSON
- No pass-level data collected

---

## 4. Instrumentation Design Gap Analysis

### 4.1 Missing Capabilities

| Feature | --mlir-timing | --diagnose | PassInstrumentation | Priority |
|---------|--------------|-----------|-------------------|----------|
| Per-pass wall-clock time | ✓ | ✗ | ✓ | HIGH |
| Per-analysis computation time | ✓ | ✗ | ✓ | MEDIUM |
| Pass IR modification detection | ✗ | ✗ | ✓ | HIGH |
| Per-pass statistics aggregation | ✗ | ✗ | ? | MEDIUM |
| Integration with --diagnose JSON | ✗ | ✗ | Possible | HIGH |
| Per-pass debug channels (--arts-debug) | ✗ | Partial | ? | LOW |

### 4.2 Why PassInstrumentation is the Right Approach

1. **Unified API**: Single registration point in PassManager
2. **Built-in threading support**: `PipelineParentInfo` tracks parent threads automatically
3. **Reference implementations**: PassTiming, IRPrinter show proven patterns
4. **Decoupling**: Instrumentation doesn't affect pass code
5. **MLIR standard**: Aligns with IREE, LLVM compiler infrastructure

---

## 5. Proposed CartsPassInstrumentation Design

### 5.1 Class Definition

```cpp
/// CartsPassInstrumentation: CARTS-specific pass instrumentation.
/// Collects per-pass timing, IR change detection, and statistics aggregation.
class CartsPassInstrumentation : public PassInstrumentation {
public:
  /// Config flags (set via CLI or AnalysisManager)
  struct Config {
    bool enableTiming = true;
    bool enableIrChangeDetection = true;
    bool enableStatisticsAggregation = true;
    bool enableAnalysisTiming = false;  // Optional, expensive
  };

  CartsPassInstrumentation(
      arts::AnalysisManager *AM,  // Optional; if null, no JSON export
      const Config &cfg = {});

  ~CartsPassInstrumentation() override = default;

  // PassInstrumentation overrides
  void runBeforePass(Pass *pass, Operation *op) override;
  void runAfterPass(Pass *pass, Operation *op) override;
  void runAfterPassFailed(Pass *pass, Operation *op) override;
  void runBeforeAnalysis(StringRef name, TypeID id, Operation *op) override;
  void runAfterAnalysis(StringRef name, TypeID id, Operation *op) override;

  // Data export
  void exportToJson(llvm::raw_ostream &os) const;

private:
  /// Per-pass timing data
  struct PassMetrics {
    std::string passName;
    std::string passArgument;
    uint64_t executionTimeMs = 0;
    int64_t operationCount = 0;  // Ops processed by this pass
    bool modifiedIr = false;
    int64_t statisticsCount = 0;  // Number of statistics incremented
  };

  /// Per-analysis timing data
  struct AnalysisMetrics {
    std::string analysisName;
    uint64_t computationTimeMs = 0;
    int64_t callCount = 0;
  };

  arts::AnalysisManager *analysisManager;  // Non-owning
  Config config;

  // Thread-local state (per llvm::get_threadid())
  DenseMap<uint64_t, SmallVector<PassMetrics, 16>> passMetricsPerThread;
  DenseMap<uint64_t, SmallVector<AnalysisMetrics, 8>> analysisMetricsPerThread;

  // Snapshots before each pass for change detection
  DenseMap<Pass *, OperationFingerPrint> beforePassFingerprints;

  // Global aggregation (guarded by mutex for thread safety)
  mutable llvm::sys::SmartMutex<true> metricsLock;
  SmallVector<PassMetrics, 32> aggregatedPassMetrics;
  SmallVector<AnalysisMetrics, 16> aggregatedAnalysisMetrics;
};
```

### 5.2 Method Signatures and Behavior

#### 5.2.1 `runBeforePass(Pass *pass, Operation *op)`

```cpp
void CartsPassInstrumentation::runBeforePass(Pass *pass, Operation *op) {
  if (isa<OpToOpPassAdaptor>(pass))
    return;  // Skip adaptors, only measure actual passes

  auto tid = llvm::get_threadid();
  auto &metrics = passMetricsPerThread[tid];

  // Record pass metadata
  PassMetrics m;
  m.passName = pass->getName().str();
  m.passArgument = pass->getArgument().str();

  // Snapshot IR if change detection enabled
  if (config.enableIrChangeDetection)
    beforePassFingerprints.try_emplace(pass, op);

  // Record pre-execution operation count
  m.operationCount = 0;
  op->walk([&](Operation *) { m.operationCount++; });

  // Record initial statistic values (if enabled)
  if (config.enableStatisticsAggregation) {
    m.statisticsCount = getActiveStatisticsCount();  // TBD helper
  }

  // Start timing
  m.executionStartTime = std::chrono::high_resolution_clock::now();

  metrics.push_back(m);
}
```

#### 5.2.2 `runAfterPass(Pass *pass, Operation *op)`

```cpp
void CartsPassInstrumentation::runAfterPass(Pass *pass, Operation *op) {
  if (isa<OpToOpPassAdaptor>(pass))
    return;

  auto tid = llvm::get_threadid();
  auto &metrics = passMetricsPerThread[tid];
  assert(!metrics.empty() && "expected active pass metrics");

  PassMetrics &m = metrics.back();

  // Record end time
  auto endTime = std::chrono::high_resolution_clock::now();
  m.executionTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
      endTime - m.executionStartTime).count();

  // Check IR modification
  if (config.enableIrChangeDetection) {
    auto it = beforePassFingerprints.find(pass);
    if (it != beforePassFingerprints.end()) {
      m.modifiedIr = (it->second != OperationFingerPrint(op));
      beforePassFingerprints.erase(it);
    }
  }

  // Aggregate into global metrics (thread-safe)
  {
    llvm::sys::ScopedLock lock(metricsLock);
    aggregatedPassMetrics.push_back(m);
  }

  metrics.pop_back();
}
```

#### 5.2.3 `runAfterPassFailed(Pass *pass, Operation *op)`

```cpp
void CartsPassInstrumentation::runAfterPassFailed(Pass *pass, Operation *op) {
  // Same as runAfterPass, but mark as failed
  if (isa<OpToOpPassAdaptor>(pass))
    return;

  auto tid = llvm::get_threadid();
  auto &metrics = passMetricsPerThread[tid];
  if (!metrics.empty()) {
    metrics.back().status = "FAILED";
  }

  runAfterPass(pass, op);
}
```

#### 5.2.4 Analysis Hooks (Optional, Expensive)

```cpp
void CartsPassInstrumentation::runBeforeAnalysis(
    StringRef name, TypeID id, Operation *op) {
  if (!config.enableAnalysisTiming)
    return;

  auto tid = llvm::get_threadid();
  auto &metrics = analysisMetricsPerThread[tid];

  AnalysisMetrics m;
  m.analysisName = name.str();
  m.analysisTypeId = id.getAsOpaquePointer();
  m.computationStartTime = std::chrono::high_resolution_clock::now();

  metrics.push_back(m);
}

void CartsPassInstrumentation::runAfterAnalysis(
    StringRef name, TypeID id, Operation *op) {
  if (!config.enableAnalysisTiming)
    return;

  auto tid = llvm::get_threadid();
  auto &metrics = analysisMetricsPerThread[tid];
  if (!metrics.empty()) {
    auto &m = metrics.back();
    auto endTime = std::chrono::high_resolution_clock::now();
    m.computationTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - m.computationStartTime).count();

    {
      llvm::sys::ScopedLock lock(metricsLock);
      aggregatedAnalysisMetrics.push_back(m);
    }
    metrics.pop_back();
  }
}
```

#### 5.2.5 JSON Export

```cpp
void CartsPassInstrumentation::exportToJson(llvm::raw_ostream &os) const {
  llvm::sys::ScopedLock lock(metricsLock);

  os << "{\n";
  os << "  \"passes\": [\n";

  bool first = true;
  for (const auto &m : aggregatedPassMetrics) {
    if (!first) os << ",\n";
    first = false;

    os << "    {\n";
    os << "      \"name\": \"" << escapeJsonString(m.passName) << "\",\n";
    os << "      \"argument\": \"" << escapeJsonString(m.passArgument) << "\",\n";
    os << "      \"time_ms\": " << m.executionTimeMs << ",\n";
    os << "      \"modified_ir\": " << (m.modifiedIr ? "true" : "false") << ",\n";
    os << "      \"operation_count\": " << m.operationCount << "\n";
    os << "    }";
  }

  os << "\n  ]\n";
  os << "}\n";
}
```

### 5.3 Integration Points in Compile.cpp

#### 5.3.1 Registration (around line 572)

```cpp
static LogicalResult configurePassManager(
    PassManager &pm,
    arts::AnalysisManager *AM = nullptr,  // NEW parameter
    bool enableCartsInstrumentation = false) {  // NEW parameter

  pm.enableVerifier(true);
  if (failed(applyPassManagerCLOptions(pm)))
    return failure();

  // Enable MLIR built-in timing
  applyDefaultTimingPassManagerCLOptions(pm);

  // Register CARTS custom instrumentation
  if (enableCartsInstrumentation || Diagnose) {
    auto carts_inst = std::make_unique<arts::CartsPassInstrumentation>(
        AM,
        arts::CartsPassInstrumentation::Config{
            .enableTiming = true,
            .enableIrChangeDetection = Diagnose,
            .enableStatisticsAggregation = Diagnose,
            .enableAnalysisTiming = false  // Disabled by default (expensive)
        });
    pm.addInstrumentation(std::move(carts_inst));
  }

  return success();
}
```

#### 5.3.2 CLI Flags (around line 140)

```cpp
static cl::opt<bool> EnablePassInstrumentation(
    "pass-instrumentation",
    cl::desc("Enable per-pass instrumentation (timing, change detection)"),
    cl::init(false));

static cl::opt<bool> EnableAnalysisInstrumentation(
    "analysis-instrumentation",
    cl::desc("Enable per-analysis timing (expensive, use only for profiling)"),
    cl::init(false));
```

#### 5.3.3 JSON Export (around line 1377)

```cpp
if (Diagnose && AM) {
  bool includeAnalysis = shouldExportDetailedDiagnose(*resolvedStopAt) &&
                         AM->hasCapturedDiagnostics();
  if (!DiagnoseOutput.empty()) {
    std::error_code EC;
    llvm::raw_fd_ostream outputFile(DiagnoseOutput, EC);
    if (EC) {
      ARTS_ERROR("Could not open diagnostics output file: " << DiagnoseOutput);
      return 1;
    }
    AM->exportToJson(outputFile, includeAnalysis);

    // NEW: Also export pass instrumentation data
    outputFile << ",\n  \"pass_instrumentation\": ";
    instrumentationInstance->exportToJson(outputFile);

    outputFile.close();
  }
}
```

---

## 6. Implementation Effort Estimate

### Phase 1: Core Implementation (2-3 days)
1. Create `include/arts/utils/PassInstrumentation.h` header
2. Implement `CartsPassInstrumentation` class
3. Add to Compile.cpp registration
4. Unit tests with simple pass

**Risk**: Medium (thread-safety of DenseMap with multiple threads)

### Phase 2: Integration with --diagnose (1-2 days)
1. Merge pass metrics into AnalysisManager::exportToJson()
2. Add pass timing to JSON schema
3. CLI flags (--pass-instrumentation, --analysis-instrumentation)

**Risk**: Low (clean integration point)

### Phase 3: Advanced Features (1-2 days, optional)
1. Statistics aggregation (requires llvm::Statistic knowledge)
2. Analysis timing (expensive, disabled by default)
3. Change detection heuristics refinement

**Risk**: Low (all optional features, no breaking changes)

### Total: 4-7 days for full implementation

---

## 7. Validation Plan

### 7.1 Correctness Tests
```bash
carts compile jacobi2d.mlir --pass-instrumentation -o /tmp/diag.json
# Verify: JSON contains all pass metrics
```

### 7.2 Performance Tests
```bash
carts compile jacobi2d.mlir --mlir-timing --pass-instrumentation
# Compare MLIR built-in timing vs. CARTS timing (should match ±10ms)
```

### 7.3 Thread Safety Tests
```bash
# Enable --mlir-num-workers (future, when threading enabled)
# Verify: No race conditions or assertion failures
```

---

## 8. Design Decisions & Trade-offs

### 8.1 Use PassInstrumentation vs. Custom Hooks

**Decision**: Use PassInstrumentation (MLIR standard)

**Rationale**:
- Proven API with reference implementations
- Automatic thread tracking via PipelineParentInfo
- Avoids reimplementing threading logic
- Aligns with IREE and LLVM best practices

### 8.2 Timing Mechanism

**Decision**: `std::chrono::high_resolution_clock` (wall-clock time)

**Alternative**: `mlir::TimingScope` (matches --mlir-timing)

**Rationale**:
- Wall-clock time more intuitive for users
- Can integrate with TimingScope later if needed
- `std::chrono` is standard library, no MLIR dependency

### 8.3 Change Detection Strategy

**Decision**: Fingerprinting (expensive but accurate)

**Alternative**: Heuristic (SSA value count, operations count)

**Rationale**:
- OperationFingerPrint matches MLIR's IRPrinter approach
- Disabled by default (only in --diagnose mode)
- Accurate detection of semantic changes

### 8.4 Statistics Aggregation

**Decision**: Deferred to Phase 3

**Rationale**:
- Requires llvm::Statistic integration (complex)
- Not critical for initial --diagnose integration
- Can be added without API changes

---

## 9. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| DenseMap corruption with threads | Medium | CRITICAL | Use llvm::sys::SmartMutex<true>, test with MLIR thread test suite |
| Fingerprinting performance regression | Medium | HIGH | Disable by default, profile with large IR |
| JSON schema drift from AnalysisManager | Low | MEDIUM | Document schema in docs/, add schema versioning |
| Integration conflicts with --mlir-timing | Low | MEDIUM | Ensure both instrumentations coexist peacefully (they do in MLIR) |

---

## 10. Recommended Next Steps

1. **Create issue**: "Implement CartsPassInstrumentation for per-pass metrics collection"
2. **Assign Phase 1**: Core implementation (2-3 days)
3. **Assign Phase 2**: Integration with --diagnose (1-2 days)
4. **Hold Phase 3**: Optional features until Phase 1-2 are stable

---

## 11. References

- **MLIR PassInstrumentation**: `external/Polygeist/llvm-project/mlir/include/mlir/Pass/PassInstrumentation.h`
- **PassTiming Reference**: `external/Polygeist/llvm-project/mlir/lib/Pass/PassTiming.cpp`
- **IRPrinter Reference**: `external/Polygeist/llvm-project/mlir/lib/Pass/IRPrinting.cpp`
- **CARTS Compile.cpp**: `tools/compile/Compile.cpp:572-578, 1360-1395`
- **AnalysisManager**: `include/arts/analysis/AnalysisManager.h:78-85`

---
