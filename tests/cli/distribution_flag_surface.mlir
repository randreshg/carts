// The positive distribution opt-in was removed. Multinode builds distribute by
// default; --no-distributed-db is the only distribution knob (origin-node
// baseline). The old --distributed-db flag no longer exists.

// RUN: not %carts-compile --distributed-db --print-pipeline-manifest-json 2>&1 \
// RUN:   | %FileCheck %s --check-prefix=REMOVED
// RUN: %carts-compile --help 2>&1 | %FileCheck %s --check-prefix=HELP

// REMOVED: Unknown command line argument '--distributed-db'
// REMOVED: Did you mean '--no-distributed-db'?

// Help advertises only the negative baseline escape and states distribution is
// the default.
// HELP: --no-distributed-db
// HELP-SAME: Disable default distribution
