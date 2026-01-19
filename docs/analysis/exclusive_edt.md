# DB Acquire/Release + Exclusive EDT

This document re-evaluates all assumptions around DB requests in ARTS and
explains when the "exclusive EDT" tracking is required. It distinguishes
acquire-time data fetches from release-time updates, and documents the exact
call chains used by jacobi/for.

All statements below are derived from runtime code:
- `external/arts/core/src/runtime/memory/DbFunctions.c`
- `external/arts/core/src/runtime/network/RemoteFunctions.c`
- `external/arts/core/src/runtime/memory/DbList.c`
- `external/arts/core/src/gas/RouteTable.c`
- `external/arts/core/src/gas/OutOfOrder.c`
- `external/arts/core/src/runtime/Runtime.c`

## Key Data Structures (Runtime Truth)

```
artsEdtDep_t:
  guid, ptr, mode, acquireMode, useTwinDiff

artsDbFrontier:
  list[]      // rank-only destinations for aggregated READ
  exNode      // rank of exclusive requester
  exEdtGuid   // EDT guid to satisfy (acquire path)
  exSlot      // slot to satisfy (acquire path)
  exMode      // dep mode
```

Important: `acquireMode` overrides `mode` at runtime when set.
This is `effectiveMode = acquireMode != ARTS_NULL ? acquireMode : mode`.

## Compile-Time -> Runtime Dependency Registration

```
MLIR: arts.db_acquire (mode=read/write)
  |
  v
EdtLowering: arts.record_dep / arts.record_dep_at
  |
  v
ConvertArtsToLLVM: artsRecordDep(...) or artsRecordDepAt(...)
  |
  v
Runtime: artsRecordDep
  - adds dependence to DB persistent event
  - increments latch if acquireMode == WRITE
```

When the EDT becomes ready, `artsHandleReadyEdt()` calls `acquireDbs(edt)`,
which decides how to fetch each DB.

## Acquire Phase: What Actually Happens

### Case A: READ (non-owner)

```
acquireDbs
  -> artsRemoteDbRequest(..., agg=true)
        |
        v
  artsRouteTableAddSent
    - stores (edt, slot) in OO list
    - sends network request only once
        |
        v
  Owner sends DB (artsRemoteDbSend/artsRemoteDbSendCheck)
        |
        v
  Receiver updates route table
  artsRouteTableFireOO -> artsDbRequestCallback(edt, slot)
```

Key point: the request packet does *not* carry edtGuid/slot; those live in the
requester's out-of-order list.

### Case B: WRITE (non-owner), twin-diff ON

Acquire still fetches the full DB, but uses the aggregated request path:

```
acquireDbs
  -> artsRemoteDbRequest(..., acquireMode=WRITE, useTwinDiff=true)
        |
        v
  Owner sends full DB via artsRemoteDbSendCheck
  (same flow as READ for delivery)
```

Twin-diff affects release (diff vs full update), not the initial fetch.

### Case C: WRITE (non-owner), twin-diff OFF

Acquire uses a *full-DB request* with explicit EDT targeting:

```
acquireDbs
  -> artsRemoteDbFullRequest(dbGuid, owner, edtGuid, slot, mode)
        |
        v
  Owner: artsRemoteDbFullSendCheck(...)
        |
        +-> artsAddDbDuplicate(...)  // records exEdtGuid/exSlot
        +-> artsRemoteDbFullSendNow  // sends full DB payload
        +-> artsClearExclusiveRequest (fix)
        |
        v
  Receiver: artsRemoteHandleDbFullRecieved
        |
        v
  artsDbRequestCallback(edt, slot)
```

This is where the exclusive request is required: the runtime must satisfy a
specific EDT slot, not just a rank.

### Owner Fetching a Valid Copy from Another Rank

If the owner rank does not have the valid copy, it requests from `validRank`:

```
owner in acquireDbs:
  if read-like -> artsRemoteDbRequest
  else          -> artsRemoteDbFullRequest(edtGuid, slot)
```

This is still an *acquire* path tied to a specific EDT slot.

## Release Phase: What Actually Happens

Release is *not* tied to a specific EDT slot. It only updates ownership and
frontier progress.

```
releaseDbs
  if WRITE:
    if owner == local:
      - artsProgressFrontier(db, local)
      - artsDbDecrementLatch
    else:
      - artsRemoteUpdateDb(dbGuid, sendDb=true)  // full update to owner
```

With twin-diff:
  - `artsTryReleaseTwinDiff` may send diffs or fall back to full update.
  - Still no EDT slot targeting in release.

## Two Different "Full DB" Transfers (Do Not Confuse Them)

1) **Acquire Full DB** (owner -> requester)
   - Packet: `ARTS_REMOTE_DB_FULL_SEND_MSG`
   - Purpose: satisfy an EDT slot
   - Uses `edtGuid` + `slot`

2) **Release Full DB Update** (requester -> owner)
   - Packet: `ARTS_REMOTE_DB_UPDATE_MSG`
   - Purpose: update canonical owner copy
   - No EDT slot involved

The "exclusive EDT" is only for (1).

## Aggregation vs Exclusive (Different Mechanisms)

- Aggregation (READ + twin-diff acquire):
  - `artsRouteTableAddSent` stores `(edt, slot)` in OO list
  - Only first request triggers network send
  - Response uses OO list to signal all waiting EDTs

- Exclusive (WRITE acquire, full-DB request):
  - `artsAddDbDuplicate` records `exEdtGuid/exSlot` in frontier
  - Full DB send targets one EDT slot
  - Must be cleared after send to avoid duplicates

## "Exclusive" Lock vs Exclusive EDT

`frontierAddExclusiveLock` is present but not used by current call sites.
All runtime paths use `exclusive=false` in `artsAddDbDuplicate`.
The exclusive EDT tracking (ex*) is independent and is triggered by
`write && !local` in `artsPushDbToFrontier`.

## jacobi/for WRITE Acquire Call Chain (MLIR -> Runtime)

```
MLIR: arts.db_acquire [mode=write]
  |
  v
EdtLowering: arts.record_dep
  |
  v
ConvertArtsToLLVM: artsRecordDep(...)
  |
  v
Runtime: artsRecordDep
  - adds persistent event dep
  - increments latch for WRITE
  |
  v
EDT ready -> artsHandleReadyEdt -> acquireDbs(edt)
  |
  v
Non-owner + WRITE + useTwinDiff=false
  -> artsRemoteDbFullRequest(dbGuid, owner, edtGuid, slot, mode)
```

## Old vs New Behavior (Exclusive Full-DB Acquire)

### Old Behavior (Before Fix)

```
Non-owner WRITE acquire
  -> artsRemoteDbFullRequest(...)
  -> owner sends full DB
  -> frontier keeps exEdt (pointer only)
  -> later: frontier progresses
  -> full DB re-sent to stale EDT pointer
  -> "Full DB received for missing EDT"
```

Root causes:
  - exclusive request stored only as EDT pointer
  - exclusive request remained after full DB send

### New Behavior (After Fix)

```
Non-owner WRITE acquire
  -> artsRemoteDbFullRequest(..., edtGuid, slot)
  -> owner sends full DB
  -> frontier stores exEdtGuid + exSlot (not just pointer)
  -> artsClearExclusiveRequest(...) after successful send
  -> no duplicate full-DB send on later frontier progress
```

Fixes:
  - store `exEdtGuid` alongside EDT pointer
  - clear exclusive request immediately after full DB send

## Assumptions Verified Against Code

1) Acquire and release are distinct; only acquire targets EDT slots.
2) Twin-diff affects release updates, not the acquire payload.
3) Full-DB acquire uses explicit EDT slot (packet carries edtGuid/slot).
4) Aggregated requests store EDT/slot locally via OO list.
5) Exclusive EDT tracking is tied to `write && !local` in frontier logic.
