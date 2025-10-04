# Security Analysis of Mojo Core: SharedBufferDispatcher

**File:** `mojo/core/shared_buffer_dispatcher.cc`

## 1. Overview

The `SharedBufferDispatcher` is the Mojo `Dispatcher` implementation for shared memory handles (`MojoHandle` of type shared buffer). It acts as a wrapper around the `base::subtle::PlatformSharedMemoryRegion` object, which abstracts the platform-specific details of creating and managing shared memory segments (e.g., POSIX `shm_open`/`mmap`, Windows `CreateFileMapping`/`MapViewOfFile`).

This dispatcher is the concrete implementation for all `Mojo...SharedBuffer...` API calls. Shared memory is a powerful but notoriously dangerous IPC mechanism. Any vulnerability in this dispatcher could lead to information disclosure, memory corruption, or sandbox escapes. Therefore, its design and implementation are critical to the security of the entire Mojo IPC system.

## 2. Core Security Model & State Management

The security model of the `SharedBufferDispatcher` revolves around two key principles: **thread safety** and **strict access control management**, especially during duplication.

*   **State**: The core state of the dispatcher is a single `region_` of type `base::subtle::PlatformSharedMemoryRegion`. This object encapsulates the platform handle(s) and metadata (size, GUID, access mode) for the memory segment.

*   **Locking**: All access to the internal state (`region_` and the `in_transit_` flag) is protected by a `lock_`. This is essential to prevent race conditions where one thread might try to map or close a handle while another thread is serializing it for transit to another process.

*   **Transit State**: The `in_transit_` boolean flag is a crucial part of the lifecycle management. When a shared buffer handle is attached to a message, `BeginTransit()` is called, which sets `in_transit_` to `true`. This effectively locks the dispatcher, preventing any further local operations (like mapping, duplicating, or closing) until the transit is either completed (at which point the dispatcher is destroyed) or canceled. This prevents use-after-free and double-use vulnerabilities.

## 3. Security-Critical Operations

### Deserialization (`SharedBufferDispatcher::Deserialize`)

This static method is a primary security boundary, responsible for reconstructing a `SharedBufferDispatcher` from data received over an untrusted IPC channel.

**Key Security Checks:**

1.  **Payload Size Validation**: It immediately checks if the incoming data size exactly matches `sizeof(SerializedState)`. Any mismatch indicates a malformed message, and deserialization is aborted.
2.  **Handle Count Validation**: This is a critical platform-dependent check. The dispatcher knows how many platform handles to expect for a given access mode on a given OS. For example, on Linux, a `kWritable` region requires two file descriptors, while a `kReadOnly` or `kUnsafe` one requires only one. The code explicitly checks if `num_platform_handles` matches the expected number. This prevents attackers from confusing the logic by providing an incorrect number of handles.
3.  **Access Mode Validation**: The `access_mode` field from the serialized data is checked to ensure it's one of the known, valid enum values.
4.  **GUID and Size Validation**: It ensures the deserialized `num_bytes` is non-zero and that the GUID is a valid `base::UnguessableToken`.

If any of these checks fail, the function returns `nullptr`, and the invalid handles are discarded, preventing a partially-initialized or corrupted dispatcher from entering the system.

### Handle Duplication (`DuplicateBufferHandle`)

This function implements the logic for `MojoDuplicateBufferHandle` and is the most complex and security-sensitive part of the dispatcher. It enforces a strict state machine to prevent dangerous aliasing of writable memory.

**Access Mode State Machine:**

A shared buffer can be in one of three modes: `kReadOnly`, `kWritable`, or `kUnsafe`.

1.  **Requesting a Read-Only Duplicate**:
    *   If the original is already `kReadOnly`, this is trivial.
    *   If the original is `kWritable`, it is **permanently demoted to `kReadOnly`** before the duplicate is created. This is a critical security guarantee: once a read-only view of a buffer has been shared, the original owner can no longer write to it, preventing a Time-of-check to time-of-use (TOCTOU) vulnerability where a malicious process could modify the buffer after a trusted process has validated its contents.
    *   If the original is `kUnsafe`, creating a read-only duplicate is forbidden. This is because an "unsafe" handle implies other writable aliases may exist, making a "safe" read-only view impossible to guarantee.

2.  **Requesting a Writable Duplicate**:
    *   If the original is `kReadOnly`, this is forbidden. You cannot upgrade a read-only handle.
    *   If the original is `kWritable`, both the original and the new duplicate are **demoted to `kUnsafe`**. This acknowledges that there are now multiple writable aliases for the same memory region. The `kUnsafe` mode serves as a "taint" flag, indicating that the application is now responsible for memory synchronization and that no "safe" read-only views can ever be created from these handles.

This state machine robustly prevents **aliased writability**, a common source of bugs where multiple actors can write to the same memory without a clear ownership model.

## 4. Potential Vulnerabilities & Mitigations

*   **Race Conditions**:
    *   **Threat**: Using a handle on one thread while it's being closed, duplicated, or serialized on another.
    *   **Mitigation**: The `lock_` and the `in_transit_` flag provide strong protection against these scenarios.

*   **Logic Bugs in Duplication State Machine**:
    *   **Threat**: A flaw in the access mode transitions in `DuplicateBufferHandle` could violate the security invariants (e.g., allowing a read-only and a writable handle to the same buffer to coexist).
    *   **Mitigation**: The logic is contained entirely within this one function and is designed to be conservative, always failing a request rather than entering an insecure state.

*   **Resource Exhaustion**:
    *   **Threat**: A malicious process could request a shared buffer of enormous size.
    *   **Mitigation**: `SharedBufferDispatcher::Create` checks the requested `num_bytes` against a configurable limit (`GetConfiguration().max_shared_memory_num_bytes`), rejecting requests that are too large.

## 5. Conclusion

The `SharedBufferDispatcher` is an excellent example of a security-hardened IPC component. It demonstrates a deep understanding of the risks associated with shared memory by enforcing a strict, conservative state machine for access control. The clear ownership model, thread-safe design, and rigorous validation during deserialization provide multiple layers of defense. The most critical security feature is the access-mode demotion logic in `DuplicateBufferHandle`, which correctly prevents dangerous aliasing of writable memory.