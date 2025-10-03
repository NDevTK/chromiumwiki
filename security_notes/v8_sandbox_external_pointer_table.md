# Security Notes: `v8/src/sandbox/external-pointer-table.cc`

## File Overview

This file implements the `ExternalPointerTable` (EPT), which is the absolute core of the V8 Sandbox security mitigation. The V8 Sandbox is designed to contain memory corruption vulnerabilities. Its primary goal is to ensure that even if an attacker achieves arbitrary read/write within the V8 heap, they cannot corrupt memory outside of that heap to take control of the process. This file manages the lifecycle of the table that makes this possible.

## The Fundamental Security Invariant

The V8 Sandbox operates on a simple but powerful principle: **there are no raw pointers from the V8 heap to the outside world.**

1.  The V8 heap is allocated within a large, contiguous virtual memory region (the "sandbox").
2.  Any V8 object that needs to reference memory outside this sandbox (e.g., a C++ object in Blink, a TypedArray's backing store) does not store a direct 64-bit pointer.
3.  Instead, it stores a 32-bit `ExternalPointerHandle`.
4.  This handle is an index into the `ExternalPointerTable`, which is a large array of 64-bit pointer-sized slots.
5.  Crucially, the `ExternalPointerTable` itself is allocated **outside** the V8 sandbox, making it immune to corruption from a compromised V8 heap.

When V8 needs to access an external object, it performs an indirection: it reads the handle from the V8 heap, uses it as an index into the EPT, and loads the true 64-bit pointer from the table. This indirection is the entire basis of the sandbox's protection.

## Key Security-Relevant Mechanisms in This File

This file is primarily concerned with the garbage collection of the `ExternalPointerTable`. Since the table stores pointers, it must be cleaned up when the objects pointing to it are no longer live. This process is complex and security-critical.

### 1. Mark-Sweep-Compact Garbage Collection

The `EvacuateAndSweepAndCompact` function implements a full garbage collection cycle for the EPT, which mirrors the main V8 garbage collector.

-   **Marking (Implicit)**: The V8 garbage collector is responsible for marking live objects. As it does so, it also marks the corresponding entries in the EPT by setting a "mark bit" in the entry's payload. This file consumes the result of that marking phase.

-   **Sweeping**: The core loop in `EvacuateAndSweepAndCompact` iterates through the table's segments. For each entry, it checks if the mark bit is set (`!payload.HasMarkBitSet()`).
    -   If an entry is **not marked**, it is considered dead. The pointer it holds is discarded, and the entry is added to a `freelist` to be reused.
    -   **Security Implication**: This is the most critical part of the security model. If the sweeping logic is flawed (e.g., it incorrectly identifies a live entry as dead), it will free the entry, leading to a classic **use-after-free**. The V8 heap will still hold a handle to the now-freed slot, and a later allocation could fill that slot with a different pointer, leading to type confusion and a sandbox escape.

### 2. Compaction and Evacuation

To fight fragmentation, the table can be compacted. This involves moving live entries to create contiguous free space.

-   **`EvacuationEntry`**: When a live entry is moved from an old location to a new one, the old slot is overwritten with an `EvacuationEntry`. This special record contains a pointer to the `ExternalPointerHandle` *inside the V8 heap*.
-   **`ResolveEvacuationEntryDuringSweeping`**: When the sweeper encounters an `EvacuationEntry`, it knows that the original entry was moved. It uses the pointer stored in the evacuation entry to find the handle in the V8 heap and **update it** to point to the new location.
-   **Security Implication**: This "patching" of the V8 heap is an extremely sensitive operation. The comment `// We must have a valid handle here. If this fails, it might mean that an // object with external pointers was in-place converted to another type of // object without informing the external pointer table.` highlights a critical risk. If the pointer to the handle is stale or corrupted, the heap patch could fail or write the new handle to an incorrect location, leading to severe memory corruption.

### 3. Thread Safety

-   During the GC cycle, allocations from other threads are forbidden. The code enforces this by setting the freelist head to a sentinel value (`kEntryAllocationIsForbiddenMarker`) and protecting the table with a `MutexGuard`. This prevents race conditions that could corrupt the freelist and the table's structure.

## Summary of Security Posture

`external-pointer-table.cc` is the implementation of a world-class security mitigation.

-   **Effectiveness**: The EPT fundamentally changes the game for attackers. A typical memory corruption bug inside V8 no longer grants arbitrary read/write across the entire process, but is instead contained within the V8 heap.
-   **Complexity as a Weakness**: The primary weakness of this system is its own complexity. The garbage collection, and especially the compaction/evacuation logic, is a highly intricate dance between the V8 heap and the external table. A single logical flaw in this process can undermine the entire security guarantee.
-   **Audit Focus**: For a security researcher, this file is a prime target. The focus should be on corner cases in the GC logic:
    -   Can an `EvacuationEntry` be processed incorrectly?
    -   Can an entry be freed while still being accessible?
    -   Are there race conditions that bypass the locks?
    -   How are object type changes (in-place conversions) handled to ensure the EPT is always notified?

Any change to this file requires the utmost scrutiny, as a bug here could render one of V8's most important security features ineffective.