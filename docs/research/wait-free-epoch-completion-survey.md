# Wait-Free Epoch/Barrier/Phase Completion in Task-Based Runtimes

## Survey for ARTS Optimization

Date: 2026-03-18

---

## 1. Comparison Table

```
Runtime      | Wait mechanism           | Continuation trigger                | Distributed protocol                  | Per-sync overhead         | Key technique
-------------|--------------------------|-------------------------------------|---------------------------------------|---------------------------|----------------------------------------------
ARTS         | Blocking poll            | Finish-EDT signal (unused today)    | 2-phase epoch reduction               | ~100+ atomics/yields      | Has finish-EDT plumbing but defaults to wait
Cilk         | Stalling join (no block) | Last-arriving worker resumes cont.  | N/A (shared memory only)              | 2-6x function call (~50ns)| THE protocol + continuation stealing
HClib        | help_finish (work-steal) | Atomic counter reaches 0            | MPI/UPC++ module (not built-in)       | ~200-500ns (est.)         | Work-first/help-first scheduling
X10/APGAS    | Blocking finish          | Counter-based TD fires callback     | Task-balancing counting protocol      | O(places) messages        | Distributed finish store with replication
Legion/Realm | Never blocks (deferred)  | Event trigger (last arrival)        | Active msgs, 2*(N-1) per trigger      | ~329ns local, ~3us remote | Generational events + phase barriers
Charm++      | Msg-driven (no block)    | Callback/SDAG continuation          | Global msg count (QD) or tree reduce  | Reduction: few us; QD: ms | Completion Detection (local alternative to QD)
PaRSEC       | Never blocks (dataflow)  | Atomic dep-counter reaches 0        | Fully distributed PTG unfolding       | ~100-300ns per task        | PTG: zero-copy dependency discovery
HPX          | Never blocks (dataflow)  | Future resolution fires continuations| AGAS + active messages               | ~700ns per thread lifecycle| hpx::dataflow avoids all suspension
OpenMP 5.0   | Event-driven (detach)    | omp_fulfill_event completes task    | Not specified (impl-dependent)        | 1 atomic + flag check     | Detachable tasks with explicit event fulfillment
```

---

## 2. Detailed Analysis Per Runtime

### 2.1 Cilk (MIT/Intel/OpenCilk)

**Does it block worker threads?** No. When a worker reaches `cilk_sync` and is not the last
to arrive (join counter != 0), it enters the steal loop to find other work. No worker ever
sleeps or spins waiting for children. The "stalling join" means the worker abandons the
current continuation and looks for productive work elsewhere.

**Continuation trigger:** The join counter is an atomic integer per spawning frame. Each
returning child decrements it. Whoever decrements it to 0 is the "last arriver" and jumps
directly to the code after `cilk_sync`. This is zero-overhead in the common sequential case
(no steal occurred) because the THE protocol makes the fast path lock-free for the deque
owner.

**Distributed protocol:** N/A. Cilk is shared-memory only. There is research on distributed
continuation stealing via RDMA but it is not in mainline.

**Per-sync overhead:** The spawn overhead is 2-6x a function call (~10-50ns). A steal costs
~15,000 instructions (~5us). The sync itself is just an atomic decrement + conditional jump
when no steal has occurred.

**Key insight for ARTS:** The THE (Dijkstra-like) protocol makes the common case (no steal)
nearly free. The deque owner pushes/pops without any lock. Only thieves pay the
synchronization cost. ARTS could adopt this asymmetric synchronization for its ready queues.

---

### 2.2 HClib (Habanero-C Library, Rice University)

**Does it block worker threads?** Partially. The `help_finish` mechanism allows a worker at
a finish boundary to participate in work-stealing rather than pure blocking. However, the
original implementation can deadlock with nested finish scopes because the finish counter
stack is not maintained correctly across worker migrations.

**Continuation trigger:** Each finish scope has an atomic counter. When an async completes,
it decrements the counter of its enclosing finish. When the counter reaches 0, the finish
scope returns. The key contribution from Rice is the **work-first vs. help-first** policy:

- **Work-first** (Cilk-like): Push continuation onto deque, execute child immediately.
  Minimizes work overhead but can overflow stacks with deep recursion.
- **Help-first**: Push child onto deque, continue with parent's code. Better for
  async-finish (more general than spawn-sync) because it naturally handles non-nested
  parallelism.

**Distributed protocol:** HClib itself is shared-memory. Distributed support comes through
modules (MPI, UPC++, OpenSHMEM) but finish scopes do not span nodes natively.

**Per-sync overhead:** Not published precisely, but the work-stealing deque uses lock-free
CAS operations. Estimated at 200-500ns for finish scope enter/exit.

**Key insight for ARTS:** The help-first policy is better suited for async-finish semantics
than Cilk's work-first. ARTS epochs are closer to finish scopes than to spawn-sync, so
help-first scheduling may be more appropriate.

---

### 2.3 X10 / APGAS

**Does it block worker threads?** Yes. The `finish` construct blocks the calling activity
until all transitively spawned asyncs complete. X10 uses "worker-blocking" semantics at
finish boundaries.

**Continuation trigger:** Distributed termination detection (TD) using a counting protocol.
Each place maintains per-finish counters of spawned vs. completed activities. The finish
"home place" aggregates these counts.

**Distributed protocol:** The "Task Balancing" algorithm:
- Each place tracks (spawned, completed) pairs per finish scope.
- When a place becomes locally idle for a finish, it reports to the finish's home place.
- The home place detects global completion when all places report and counts balance.
- This requires O(places) messages per finish completion in the worst case.
- Resilient finish adds a backup replica at place (home+1) for fault tolerance.
- The scalable distributed finish store was later removed due to complexity/instability.

**Per-sync overhead:** O(places) messages for distributed finish. Single-node finish is
a simple atomic counter.

**Key insight for ARTS:** X10's experience shows that centralized finish stores do not
scale. Their counting protocol is similar to ARTS's 2-phase epoch reduction but more
fine-grained (per-finish rather than global). The lesson: distributed TD must be
decentralized and should avoid a single aggregation point.

---

### 2.4 Legion / Realm (Stanford)

**Does it block worker threads?** Never. Realm is fully asynchronous. Every operation
returns immediately with an event handle. Nothing ever blocks waiting for an event --
instead, dependent operations are deferred and automatically triggered when their
precondition events fire.

**Continuation trigger:** Events are distributed objects. When an event triggers:
1. The owning node notifies all local waiters (dependent operations become ready).
2. The owning node sends trigger messages to all subscribed remote nodes.
3. Remote nodes then trigger their local waiters.

Phase barriers extend this: they are multi-generation events that require a specified
number of arrivals before triggering. Operations can arrive and wait independently.
Barriers support up to 2^32 generations, enabling iterative producer-consumer patterns
without allocating new events each iteration.

**Distributed protocol:**
- Event handles encode the owner node in upper bits (no collision, no inter-node alloc).
- Remote trigger requires 2 active message flights minimum.
- Total messages per trigger bounded by 2*(N-1) where N = interested nodes.
- Subscription aggregation limits fan-out to 1 message per node even for high fan-in.

**Per-sync overhead:** 329 nanoseconds local event trigger latency. ~3 microseconds when
going from 1 to 2 nodes (dominated by GASNet active message latency). The Completion Queue
API provides batch notification (like `MPI_Testany`) for efficient polling of event groups.

**Key insight for ARTS:** Realm's deferred execution model is the gold standard for
wait-free task runtimes. The key technique is that events are first-class distributed
objects with subscriber lists. ARTS should adopt generational events for epoch phases --
this eliminates the need to allocate/destroy epoch handles for iterative computations.

---

### 2.5 Charm++

**Does it block worker threads?** No (in the message-driven model). Charm++ uses an
event-driven execution model where entry methods are invoked asynchronously. Workers
process messages from a queue and never block. The only "blocking" is in threaded entry
methods (which use user-level threads), but this is opt-in.

**Continuation trigger:** Three mechanisms:
1. **Callbacks (CkCallback):** Registered with `CkStartQD()` for quiescence or with
   reductions. The callback is a message to a specific chare entry method.
2. **Structured Dagger (SDAG):** A continuation-passing DSL embedded in .ci files.
   `when` clauses suspend the chare's control flow until specified messages arrive.
   This is compiler-generated state machines, not thread suspension.
3. **Completion Detection:** A more modular alternative to QD. Each participating object
   explicitly calls `produce()` and `consume()`. When the global count balances, a
   registered callback fires. This is O(log P) via tree reduction.

**Distributed protocol:**
- QD: Global message counting. All sent messages must be received and processed.
  Uses a spanning tree for aggregation. Cost: O(log P) rounds.
- Reduction: Tree-based, O(log P) latency, carries data.
- Completion Detection: Similar tree-based counting, O(log P).
- QD is slower than reduction by "a few times" per the docs.

**Per-sync overhead:** Reductions are a few microseconds. QD takes milliseconds due to
its global nature. Completion Detection is between the two.

**Key insight for ARTS:** Charm++'s Completion Detection is directly analogous to ARTS
epochs. The produce/consume API maps to ARTS's EDT spawn/complete tracking. The key
difference is that Charm++ fires a callback (not a blocking wait) when the count
balances. ARTS already has the `arts_signal_edt_value` mechanism for this -- it just
needs to be wired up (which is what the epoch-finish-continuation RFC proposes).

---

### 2.6 PaRSEC (ICL/UTK)

**Does it block worker threads?** Never in steady state. PaRSEC is purely dataflow-driven.
Workers pull ready tasks from scheduling queues. When no tasks are ready, workers can
spin or sleep, but they never block waiting for a specific task's completion.

**Continuation trigger:** Each task has an atomic dependency counter initialized to the
number of incoming data flows. When a predecessor task completes, it atomically decrements
the counters of all successor tasks. When a counter reaches 0, the task is placed on the
ready queue. There is no explicit "continuation" -- the DAG structure implicitly encodes
what runs next.

**Distributed protocol:** Two modes:
1. **PTG (Parameterized Task Graph):** The DAG is described symbolically. Each node
   independently computes which tasks it owns and what data to send/receive. Zero
   runtime discovery overhead. Data transfers are initiated by the producer when it
   completes a task -- no request/response protocol needed.
2. **DTD (Dynamic Task Discovery):** Each process discovers the full DAG sequentially,
   but trims non-local portions. A sliding window limits memory usage. Communication
   is delayed if the receiver hasn't discovered the target task yet.

**Per-sync overhead:** Target is "a few hundred clock cycles" (~100-300ns) per task
according to TTG work. METG at 50% efficiency is competitive with MPI for stencil
patterns. The PTG representation avoids materializing the full DAG, so there is no
per-barrier overhead -- dependencies are resolved implicitly.

**Key insight for ARTS:** PaRSEC's PTG model shows that symbolic DAG representations
can eliminate per-synchronization overhead entirely. The compiler (JDF compiler in
PaRSEC's case, CARTS compiler in ours) can generate the dependency functions at compile
time. ARTS could adopt PTG-style dependency resolution for regular patterns (stencils,
elementwise) where the successor set is known statically.

---

### 2.7 HPX (STE||AR Group)

**Does it block worker threads?** Strongly discouraged. HPX provides `future::get()` which
CAN block (suspending the user-level thread), but the recommended pattern is
`hpx::dataflow()` which never blocks. With `dataflow`, the function is only invoked once
all future arguments are ready. This is continuation-passing without explicit callbacks.

**Continuation trigger:** `hpx::dataflow()` registers the function as a continuation on
all input futures. When the last future resolves, the runtime creates a new lightweight
thread to execute the continuation. The continuation itself returns a future, enabling
chaining. This creates a dependency tree of futures that "unravels with full speed,
without any suspension or waiting."

**Distributed protocol:** HPX uses Active Global Address Space (AGAS) for distributed
object location. Futures can span nodes. Remote future resolution sends an active message
to the waiting node, which then triggers local continuations.

**Per-sync overhead:** ~700ns for a full lightweight thread lifecycle (create, schedule,
execute, destroy). The `dataflow` path avoids thread creation for intermediate
synchronization points, bringing per-sync cost close to a single atomic operation.

**Key insight for ARTS:** HPX's key innovation is `dataflow()` -- the idea that you
express synchronization as "call this function when these values are ready" rather than
"wait for these values, then call the function." This is exactly what ARTS's
finish-EDT mechanism provides, but HPX makes it the default programming model rather
than an opt-in optimization.

---

### 2.8 OpenMP 5.0 Detach Clause

**Does it block worker threads?** With the detach clause, no. A detachable task's
completion is decoupled from its structured block's return. The task is only "complete"
when BOTH the block finishes AND `omp_fulfill_event()` is called. This allows a task
to return without blocking while an asynchronous operation (GPU kernel, MPI recv, I/O)
completes later.

**Continuation trigger:** The runtime creates an event handle associated with the task.
When `omp_fulfill_event(event)` is called (potentially from a callback in another
library), the runtime marks the task as complete and releases any `taskwait` or
`taskgroup` dependencies. This is a two-condition AND: structured-block-done AND
event-fulfilled.

**Distributed protocol:** Not specified by the OpenMP standard. Implementation-dependent.

**Per-sync overhead:** One atomic flag set + one dependency check per detached task
completion. LLVM's implementation uses a detachable flag (bit 6 = 0x40) on the task
structure and the `__kmpc_task_allow_completion_event()` internal entry point.

**Key insight for ARTS:** The detach clause's two-condition completion model is useful
for mixed synchronization. ARTS could adopt a similar pattern: an EDT's "completion"
could require both its code finishing AND an external event (epoch completion, remote
data arrival). This would unify EDT completion and epoch completion into a single
mechanism.

---

## 3. Prioritized Techniques for ARTS

### Priority 1: Wire Up Finish-EDT for Epochs (from Charm++ / Realm)

**Description:** Replace `arts_wait_on_handle` blocking polls with the already-existing
`arts_initialize_and_start_epoch(finish_edt_guid, slot)` mechanism. When an epoch
completes, the runtime signals the finish EDT's control slot, which triggers the
continuation EDT via the normal dependency resolution path.

**Which runtimes use it:** Charm++ (callbacks for QD/completion detection), Realm
(event-trigger fires dependent operations), PaRSEC (dataflow completion fires successors).

**How it maps to ARTS:** The plumbing already exists in ARTS (confirmed in RFC EF-001
through EF-004). The compiler needs to:
1. Create the continuation EDT before the epoch.
2. Pass the continuation's GUID and control slot to `arts_initialize_and_start_epoch`.
3. Remove the `arts_wait_on_handle` call.
4. Move post-epoch code into the continuation EDT's body.

**Expected improvement:** Eliminates all blocking polls in epoch synchronization hot paths.
For iterative stencil codes with epochs per timestep, this removes the largest source of
idle spinning. Conservative estimate: 20-40% reduction in synchronization overhead for
epoch-heavy codes.

**Complexity:** Medium. The runtime support exists. The compiler work (continuation capture,
live variable forwarding) is the main effort. Already designed in the epoch-finish-continuation
RFC.

---

### Priority 2: Generational Events for Iterative Epochs (from Realm)

**Description:** Instead of creating and destroying epoch handles each iteration of a
timestep loop, use a single phase barrier with multiple generations. Each loop iteration
advances to the next generation. This eliminates epoch allocation/deallocation overhead
and allows the runtime to pre-register continuations for future iterations.

**Which runtimes use it:** Legion/Realm (phase barriers with up to 2^32 generations),
Charm++ (structured dagger `when` clauses across iterations).

**How it maps to ARTS:** ARTS epochs currently have no generation concept. Adding one would
require:
1. An `arts_advance_epoch(epoch_guid)` API that resets the epoch's completion state for
   the next phase.
2. Compiler detection of epoch-in-loop patterns to use generational epochs instead of
   create/destroy per iteration.
3. Pre-registration of the continuation EDT for the next generation before the current
   generation completes.

**Expected improvement:** Eliminates per-iteration epoch creation overhead (GUID allocation,
frontier registration, completion state initialization). For codes with thousands of
timesteps, this saves ~2-5us per iteration in allocation overhead alone, plus improved
cache behavior from reusing the same data structures.

**Complexity:** Hard. Requires new runtime APIs and compiler loop analysis. The Realm paper
shows this is well-understood but the implementation touches core ARTS data structures.

---

### Priority 3: THE Protocol for Ready Queues (from Cilk)

**Description:** Replace the current locking/CAS protocol on ARTS worker deques with
Dijkstra's THE protocol. The deque owner (local worker) pushes and pops without any
synchronization. Only thieves (remote workers stealing tasks) acquire a lock, and
conflicts between owner and thief are resolved by a careful protocol that avoids locks
in the common case.

**Which runtimes use it:** Cilk (all versions), OpenCilk.

**How it maps to ARTS:** ARTS already uses lock-free deques with CAS for work stealing.
The THE protocol would further optimize the local path by removing even the CAS on push/pop
for the owning worker. This matters because ARTS workers push/pop from their local deque
on every EDT completion (to schedule the next EDT).

**Expected improvement:** Reduces per-EDT scheduling overhead by eliminating 1-2 atomic
operations on the fast path. For fine-grained EDTs (sub-microsecond work), this could
improve throughput by 10-20%.

**Complexity:** Medium. The THE protocol is well-documented and Cilk's implementation is
public. The main challenge is integrating it with ARTS's existing deque structure and
ensuring correctness with ARTS's priority-aware scheduling.

---

### Priority 4: Dataflow Continuation (from HPX / PaRSEC)

**Description:** Instead of creating explicit synchronization points (epochs/barriers),
express post-synchronization work as a continuation that fires when all input data
dependencies are satisfied. This is the `hpx::dataflow()` pattern: the function is
invoked only when all futures resolve.

**Which runtimes use it:** HPX (`hpx::dataflow`), PaRSEC (atomic dep counter -> ready
queue). All purely dataflow runtimes use this pattern implicitly.

**How it maps to ARTS:** ARTS already has the EDT dependency mechanism (`depc_needed`).
The compiler could, for simple cases, replace epoch-wait patterns with direct dependency
edges:
1. Each worker EDT, upon completion, signals a slot on the continuation EDT.
2. When all slots are satisfied, the continuation EDT becomes ready.
3. No epoch is needed at all -- the dataflow graph provides the synchronization.

This is particularly effective for reduction patterns and fan-in synchronization where
the number of predecessors is known at compile time.

**Expected improvement:** Eliminates epoch overhead entirely for cases where the
predecessor set is statically known. Removes the 2-phase reduction protocol latency.
Estimated 30-50% improvement for small fan-in synchronization points.

**Complexity:** Medium-Hard. The compiler must prove that the predecessor set is complete
and statically determinable. Works well for `parallel for` reductions but not for
dynamic task spawning.

---

### Priority 5: Completion Detection with Produce/Consume (from Charm++)

**Description:** For cases where the number of tasks is not known statically (dynamic
spawning), use a produce/consume counting protocol instead of epoch-level termination
detection. Each task spawn increments a "produced" counter; each task completion
increments a "consumed" counter. When produced == consumed and no tasks are in flight,
fire the completion callback.

**Which runtimes use it:** Charm++ (Completion Detection library), X10 (finish counting
protocol).

**How it maps to ARTS:** ARTS epochs already track active EDTs. The difference is that
Charm++'s Completion Detection fires a callback (non-blocking) while ARTS blocks. This
is essentially a generalization of Priority 1 -- use the finish-EDT mechanism but with
a more efficient counting protocol.

For distributed ARTS, the tree-based aggregation (O(log P) latency) is already close
to optimal. The improvement would come from eliminating the blocking wait at the root.

**Expected improvement:** Primarily eliminates blocking at synchronization points. The
counting overhead is already present in ARTS. Net improvement: 10-20% for dynamic
task patterns.

**Complexity:** Easy-Medium. Most of the infrastructure exists. The main work is
ensuring the counting protocol is correct when tasks migrate between nodes.

---

### Priority 6: Detachable Task Completion (from OpenMP 5.0)

**Description:** Allow an EDT to declare that its completion depends on an external
event (not just its code returning). This enables "split-phase" operations where an
EDT initiates an asynchronous operation (e.g., remote data fetch) and returns
immediately, with the operation's callback later signaling completion.

**Which runtimes use it:** OpenMP 5.0 (`detach` clause), Realm (user events).

**How it maps to ARTS:** ARTS currently ties EDT completion to the return of the EDT
function. A detachable EDT would:
1. Have an associated event handle.
2. Return from its function body (releasing the worker thread).
3. Be marked "complete" only when `arts_fulfill_event(handle)` is called.
4. Dependencies on this EDT would not be satisfied until fulfillment.

This is particularly useful for overlapping computation with communication: an EDT
can initiate a remote DB fetch, return immediately, and the fetch completion handler
calls `arts_fulfill_event()`.

**Expected improvement:** Enables computation-communication overlap that is currently
impossible. Impact is application-dependent but could be 2-5x for communication-bound
codes.

**Complexity:** Hard. Requires new runtime APIs and careful integration with the EDT
lifecycle (particularly cleanup and error handling).

---

### Priority 7: Symbolic DAG / PTG Compilation (from PaRSEC)

**Description:** For regular computation patterns (stencils, matrix operations), compile
the task graph into a symbolic representation (Parameterized Task Graph) where
dependencies are computed on-demand via index arithmetic rather than stored explicitly.
This eliminates per-task dependency tracking overhead entirely.

**Which runtimes use it:** PaRSEC (PTG/JDF language), Legion (index space tasks with
implicit dependencies from region requirements).

**How it maps to ARTS:** CARTS already knows the loop structure and data access patterns
at compile time. For stencil patterns, the compiler could generate dependency functions
like `successor(i,j) = {(i-1,j), (i+1,j), (i,j-1), (i,j+1)}` instead of recording
explicit dependency edges at runtime.

**Expected improvement:** Eliminates O(tasks) dependency storage and O(deps) atomic
decrements. For large stencil codes with millions of tasks, this could reduce memory
usage by 10-100x and improve scheduling throughput significantly.

**Complexity:** Very Hard. This is essentially a new execution model that requires deep
compiler-runtime co-design. Long-term goal, not a near-term optimization.

---

## 4. Summary Matrix

```
Priority | Technique                      | Source Runtime    | Effort   | Impact  | ARTS Readiness
---------|--------------------------------|------------------|----------|---------|----------------
1        | Finish-EDT for epochs          | Charm++/Realm    | Medium   | High    | Runtime ready, compiler WIP
2        | Generational epoch events      | Realm            | Hard     | Medium  | Needs new runtime API
3        | THE protocol for deques        | Cilk             | Medium   | Medium  | Needs deque redesign
4        | Dataflow continuation          | HPX/PaRSEC       | Med-Hard | High    | Partial (EDT deps exist)
5        | Produce/consume completion     | Charm++          | Easy-Med | Medium  | Epoch counting exists
6        | Detachable EDT completion      | OpenMP 5.0/Realm | Hard     | High*   | Needs new runtime API
7        | Symbolic DAG (PTG)             | PaRSEC           | V. Hard  | V. High | Long-term research
```

*Impact is application-dependent (high for communication-bound codes).

---

## 5. Recommended Immediate Action

**Priority 1 is the clear winner:** The ARTS runtime already supports
`arts_initialize_and_start_epoch(finish_edt_guid, slot)`. The compiler just needs to use
it. This is exactly what the epoch-finish-continuation RFC (EF-001 through EF-015)
proposes. Completing that implementation should be the immediate focus.

Once Priority 1 is in place, Priority 4 (dataflow continuation) becomes the natural
next step: for statically-known fan-in patterns, bypass epochs entirely and use direct
EDT dependencies. This eliminates the epoch infrastructure overhead for the most common
synchronization pattern in scientific codes.

Priority 3 (THE protocol) is an orthogonal optimization that can be pursued in parallel
with the epoch work, as it affects the scheduler's ready queue rather than the
synchronization mechanism.

---

## Sources

- [HClib GitHub](https://github.com/habanero-rice/hclib)
- [HClib Tutorial (Rice University)](https://habanero-rice.github.io/hclib/HClibTutoria.pdf)
- [Work-First and Help-First Scheduling Policies (Barik, Guo, Raman, Sarkar)](https://www.cs.rice.edu/~yguo/pubs/PID824943.pdf)
- [X10 and APGAS at Petascale (Tardieu et al., PPoPP 2014)](https://dl.acm.org/doi/10.1145/2555243.2555245)
- [Resilient Finish Protocols (Hamouda & Milthorpe, HAL/INRIA)](https://inria.hal.science/hal-02169496/document)
- [Realm: An Event-Based Low-Level Runtime (PACT 2014)](https://cs.stanford.edu/~sjt/pubs/pact14.pdf)
- [Realm Events Tutorial](https://legion.stanford.edu/tutorial/realm/events_basic.html)
- [Realm Phase Barriers Tutorial](https://legion.stanford.edu/tutorial/realm/barrier.html)
- [Realm Completion Queue Tutorial](https://legion.stanford.edu/tutorial/realm/completion_queue.html)
- [Charm++ Documentation](https://charm.readthedocs.io/en/latest/charm++/manual.html)
- [Charm++ FAQ](https://charm.readthedocs.io/en/latest/faq/manual.html)
- [PaRSEC Overview (OLCF)](https://www.olcf.ornl.gov/wp-content/uploads/2016/01/OLFC-User-Meeting-bosilca.pdf)
- [Dynamic Task Discovery in PaRSEC (Hoque et al., 2017)](https://icl.utk.edu/files/publications/2017/icl-utk-1027-2017.pdf)
- [HPX and C++ Dataflow (STE||AR Group)](https://stellar-group.org/2015/06/hpx-and-cpp-dataflow/)
- [HPX Documentation](https://hpx-docs.stellar-group.org/latest/singlehtml/index.html)
- [OpenMP 5.0 Task Detachment Examples](https://passlab.github.io/Examples/contents/Chap_tasking/4_Task_Detachment.html)
- [LLVM OpenMP Detach Implementation (D62485)](https://reviews.llvm.org/D62485)
- [omp_fulfill_event Spec](https://www.openmp.org/spec-html/5.0/openmpsu162.html)
- [Cilk-5 Implementation Paper](https://www.researchgate.net/publication/2760161_The_Implementation_of_the_Cilk-5_Multithreaded_Language)
- [MIT 6.172 Lecture 13: The Cilk Runtime](https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/fe9b746636bea5357ea8269fe6463020_MIT6_172F18_lec13.pdf)
- [Distributed Continuation Stealing (Cluster 2022)](https://sshiina.gitlab.io/papers/cluster22.pdf)
- [Task Bench: A Parameterized Benchmark (SC 2020)](https://arxiv.org/pdf/1908.05790)
- [Task Bench GitHub](https://github.com/StanfordLegion/task-bench)
- [Event-Based OpenMP Tasks for GPU Systems](https://link.springer.com/chapter/10.1007/978-3-031-72567-8_3)
- [Quantifying Overheads in Charm++ and HPX using Task Bench](https://arxiv.org/pdf/2207.12127)
