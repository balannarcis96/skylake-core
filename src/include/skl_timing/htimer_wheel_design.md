# HTimerWheel Design

## Feature List

1. **Dedicated tick thread** - Single thread ticks the wheel, pinned to a mostly-exclusive core
2. **Burst semantics** - Collect all triggered events per tick into a batch, enqueue to consumer thread via queue
3. **Notification events** - Lightweight events carrying a `u64` handle, consumer interprets the handle
4. **Lambda events** - Callable events (lambdas/functors) executed on the main/consumer thread
5. **Optimistic short events** - Short-delay timers expected to fire (not cancelled), optimize for trigger path
6. **Pessimistic events (short & long)** - Timers frequently cancelled before expiry, optimize for cancellation path
7. **Server-grade performance** - Low latency, predictable timing, minimal allocations in hot path

---

## Design Decisions

### 2. Burst Semantics

**Reader (main thread) priority design:**

- Reader acquires lock, drains queue, releases lock
- Writer (tick thread) collects triggered events into a **local buffer** during tick
- Writer attempts lock after tick completes
  - If lock acquired: flush local buffer to shared queue
  - If contended: keep accumulating in local buffer, retry next tick (or spin briefly)

**Flow:**

```
Writer (tick thread):
  1. tick() -> collect expired entries into thread-local buffer
  2. try_lock(shared_queue)
     - success: move local buffer contents to shared queue, unlock
     - fail: continue (will retry next tick)

Reader (main thread):
  1. lock(shared_queue)
  2. swap/drain entire queue into local processing buffer
  3. unlock
  4. process events (outside lock)
```

This minimizes lock hold time for both sides. Reader does bulk swap, writer does bulk insert.

**Implementation: `spsc_bidirectional_ring_t`**

Use the existing wait-free bidirectional SPSC ring buffer for event passing:

| Role | Timer Thread (Producer) | Main Thread (Consumer) |
|------|------------------------|------------------------|
| Send events | `allocate()` → fill → `submit()` | `dequeue_burst()` to receive |
| Return completion | `dequeue_results_burst()` to receive | `submit_results()` to send back |
| Reclaim slots | `submit_processed_results()` | - |

**Flow:**
1. Timer thread: `allocate()` expired event slots, fill them, `submit()`
2. Main thread: `dequeue_burst()` events, process them, `submit_results()`
3. Timer thread: `dequeue_results_burst()` to see completions, `submit_processed_results()` to reclaim slots

**Benefits:**
- Wait-free (no locks)
- Burst-friendly APIs built-in
- Automatic slot recycling with optional `reset()` callback
- Cache-line aligned (no false sharing)
- Fixed size (power of 2), no allocations in hot path
- Ring size acts as natural backpressure mechanism

---

### 3. Threading Model & Data Ownership

**Separation of concerns:**

| Aspect | Main Thread | Timer Thread |
|--------|-------------|--------------|
| Owns | Event data (local storage) | Wheel structure |
| Owns | Cancel flags | Handle→timeout mappings |
| Owns | GC vector | Tick/cascade logic |
| Generates | Handles (u64 counter) | Nothing |

**Key invariant:** Main thread is a tick-based worker thread.

**Handle generation:**
- Main thread generates handles via simple `u64` counter (single writer, no atomics needed)
- Timer thread only reads handles, never generates them
- Handle is just an ID; timer thread uses it as key to track/cancel events

**Event allocation flow:**
```
Main Thread:
1. handle = next_handle++          // local counter, no contention
2. slot = request_ring.allocate()  // wait-free
3. slot->handle = handle
4. slot->timeout = delay
5. request_ring.submit()
6. (for object events) store event data locally indexed by handle
```

**Timer thread as "handle scheduler":**
- Receives (handle, timeout) pairs
- Inserts into wheel
- On expiry: sends handle back to main thread via triggered ring
- Doesn't touch event data at all

---

### 4. Cancellation Design

**Optimized for main thread (pessimistic events):**

Since main thread owns event data and cancel flags, cancellation is local:

```
Main Thread:
1. Mark event as cancelled locally (flag/bitset)
2. Add handle to GC vector
3. Continue tick work...
4. End of tick: process GC vector
   - Reclaim event data locally
   - Batch submit cancel notifications to timer thread

Timer Thread:
- Dequeue cancel notifications
- Remove handles from wheel (lazy, can skip if already triggered)
```

**Race condition handling (cancel vs trigger):**
```
Timer: submits handle as triggered
Main: already marked as cancelled locally
Main: sees triggered handle → checks local flag → cancelled → skip
```

**Benefits:**
- Cancel is O(1) locally (hot path for pessimistic events)
- Timer thread cleanup is batched and async
- Race is safe: main thread always checks local cancel flag

---

### 5. Event Types (2×2 Matrix)

**Two axes:**

| Axis | Options |
|------|---------|
| Data type | Handle-only (u64 payload) vs Object (local storage with lambda/callback) |
| Cancel pattern | Optimistic (almost all fire) vs Pessimistic (most cancelled) |

**Event matrix:**

|  | Handle-only (u64 payload) | Object event (local data) |
|--|---------------------------|---------------------------|
| **Optimistic** | notify with u64, almost all fire | callback/lambda, almost all fire |
| **Pessimistic** | notify with u64, most cancelled | callback/lambda, most cancelled |

**Handle-only events:**
- Timer thread sends back the u64 directly
- Main thread interprets it (lookup, ID, pointer, whatever)
- No local storage management
- Lightweight

**Object events:**
- Main thread allocates local event data (lambda, context, etc.)
- Timer thread sends back handle
- Main thread looks up local object by handle, executes
- Needs: slot array, active bitset, GC vector

---

### 6. Data Structure Choice: Wheel vs Heap

**Decision: Hierarchical Timer Wheel**

For 50k+ events with 256hz tick rate:

| Factor | Wheel | Heap |
|--------|-------|------|
| Insert | O(1) | O(log n) |
| Cancel | O(1) with bitset | O(log n) with map |
| Tick @ 256hz | O(1) amortized | O(log n) per pop |
| Memory | sparse, pre-allocated | dense |

**Verdict:** Wheel wins for:
- High throughput (50k events)
- Frequent cancellation (pessimistic pattern)
- Regular tick interval (256hz)

Heap only makes sense for <1k events with rare cancels and irregular timing.

---

### 7. Wheel Configuration

**Tick interval:** 256hz (~4ms granularity)

**Per tick budget (~4ms):**
- Collect expired events
- Process event submissions
- Enqueue triggered events to main thread
- Reclaim completed events

**Wheel types (template parameter or separate instances):**
- `Optimistic` - tune for trigger path
- `Pessimistic` - tune for cancel path

Each handles both handle-only and object events.

---

## TODO: Next Session

### Still to design:

1. **Hierarchical wheel structure**
   - Number of levels (Linux uses 5: 256 + 64×4 slots)
   - Slots per level (power of 2 for fast modulo)
   - Time range: target ~16 days down to ~4ms granularity (256hz tick)
   - All slot counts power of 2

2. **Cascading logic**
   - When level 0 wraps, pull entries from level 1 and redistribute
   - Entry storage: handle + absolute timeout (simpler for cascading)

3. **Slot container design**
   - Reference existing `timer_wheel` implementation: `skl_vector` + `DynamicBitSet`
   - Each slot holds entries, bitset tracks enabled/cancelled

4. **Concrete API design**
   - `allocate()` / `submit()` for event creation
   - `cancel()` for cancellation
   - `tick()` for timer thread
   - Burst dequeue APIs for main thread

5. **SPSC ring integration**
   - Request ring: main → timer (new events)
   - Triggered ring: timer → main (expired handles)
   - Cancel ring: main → timer (cancel notifications)
   - Use `spsc_bidirectional_ring_t` where bidirectional needed

### Reference files:
- `/home/dev/projects/skylake-core/src/include/skl_timing/timer_wheel` - existing single-level wheel
- `/home/dev/projects/skylake-core/src/include/skl_timing/timer_wheel_handle` - handle struct
- `/home/dev/projects/skylake-core/src/include/skl_spsc_bidirectional_ring` - wait-free SPSC ring

### Coding Conventions (from DEV_PRIMER.md):

**Naming:**
- **PascalCase**: classes, enums, constexpr values
- **snake_case**: data, members, functions, POD, structs

**Prefixes:**
- `f_` - function/method parameters
- `m_` - private data members
- `E` - enum type names
- `_` - template parameter names
- `C` - constexpr constants

**Postfixes:**
- `_t` - non-class/non-enum type names (recommended)

**Method/Function comments:**
- `[ThreadSafe]` / `[ThreadUnsafe]` / `[ThreadLocal]`
- `[Callback]` / `[Internal]` / `[Util]`
- `[Compiletime]` / `[System]`
- `[Getter]` / `[Setter]` / `[Query]`
- `[Coroutine]` / `[Const]` / `[Tune]`
- `[Invariant]` / `[ATRP]` / `[DOD]`
- `[TestUtil]` / `[Cached]` / `[Async]`
- `[Allocator]` / `[KPI]` / `[Net]`
- `[SCSP]` / `[SCMP]` / `[MCSP]` / `[MCMP]`
- `[Init]` / `[Shutdown]` / `[Reset]` / `[LibInit]`

### Key decisions made:
- Timer thread only holds handles + timeouts, no event data
- Main thread owns all event data and cancel flags
- Handle = plain u64 counter (main thread generates, no atomics)
- Wheel over heap for 50k+ events at 256hz
- Cancel is local to main thread, batched notify to timer thread

---
