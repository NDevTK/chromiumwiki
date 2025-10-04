# Security Analysis of Mojo Core: The Core API

**File:** `mojo/core/core.cc`

## 1. Overview

The `Core` class, implemented in `mojo/core/core.cc`, is the heart of the Mojo system within a single process. It acts as the central nexus for all Mojo functionality, serving as the concrete implementation behind the public Mojo C API (e.g., `MojoCreateMessagePipe`, `MojoWriteMessage`, etc.). It is effectively a singleton that manages the process-global state required for Mojo to operate.

From a security perspective, `Core` is the ultimate gatekeeper. Every interaction with Mojo from the user's code, whether malicious or benign, must pass through this class. Its primary responsibilities are managing object lifetimes, enforcing security policies, and translating abstract API calls into concrete actions on the underlying IPC primitives.

## 2. Core Components & Responsibilities

The `Core` class owns and manages several critical components:

*   **`HandleTable`**: This is arguably the most important security component in the file. It maintains the mapping between the opaque `MojoHandle` (a `uint32_t`) that is exposed to the API user and the internal, strongly-typed `Dispatcher` objects that implement the handle's functionality. All access to the `HandleTable` is guarded by a lock to prevent race conditions.
    *   **Security Implication**: The integrity of the `HandleTable` is paramount. A bug here could lead to use-after-free, type confusion (e.g., treating a `DataPipeDispatcher` as a `MessagePipeDispatcher`), or handle leaks. The atomic `GetAndRemoveDispatcher` operation is crucial for safely closing handles and performing consuming operations like `FuseMessagePipes`.

*   **`NodeController`**: `Core` owns the `NodeController`, which in turn manages the `ports::Node` for the process. The `NodeController` is responsible for all cross-node (and therefore cross-process) communication logic, including peer discovery and channel management. `Core` acts as the interface between the API and the `NodeController`.

*   **`MappingTable`**: This table tracks all active shared memory mappings created via `MojoMapBuffer`. It maps the memory address returned to the user to the underlying `PlatformSharedMemoryMapping` object. This is also protected by a lock.
    *   **Security Implication**: This prevents a user from, for example, unmapping an arbitrary pointer. When `MojoUnmapBuffer` is called, the address is looked up in this table. If it's not present, the call is rejected. This prevents wild unmaps. The table also has a size limit (`max_mapping_table_size`) to mitigate denial-of-service attacks via excessive mapping requests.

## 3. Security-Critical Operations

### Handle Lifecycle Management

The `Core` class meticulously manages the lifecycle of every Mojo object through its `HandleTable`.

*   **Creation**: Functions like `MojoCreateMessagePipe` or `MojoCreateDataPipe` result in the creation of one or more `Dispatcher` objects, which are then added to the `HandleTable` via `AddDispatcher`, returning a new `MojoHandle` to the user.
*   **Usage**: Any API call that takes a `MojoHandle` (e.g., `MojoWriteMessage`) first calls `GetDispatcher` to safely look up the corresponding `Dispatcher` object. If the handle is invalid, `GetDispatcher` returns `nullptr`, and the operation fails.
*   **Destruction**: `MojoClose` uses `GetAndRemoveDispatcher` to atomically retrieve and remove the `Dispatcher` from the table, after which the `Dispatcher`'s `Close()` method is called. This two-step process ensures that no other thread can access the handle after it has been marked for closure.

### Invitation and IPC Bootstrapping

The invitation system is the mechanism for establishing a new IPC connection between two processes. This is an extremely security-sensitive process.

*   **`SendInvitation`**: This function is a major security boundary. It takes a local `InvitationDispatcher`, a target process handle, and a platform-specific transport endpoint.
    *   **Validation**: It performs rigorous validation on its arguments. It ensures the transport endpoint is valid and unwraps the process handle.
    *   **Policy Flags**: It respects security-relevant options like `MOJO_SEND_INVITATION_FLAG_UNTRUSTED_PROCESS` and `MOJO_SEND_INVITATION_FLAG_ISOLATED`. These flags are passed down to the `NodeController` to inform its policy decisions (e.g., whether to treat the connection as hostile).
    *   **Error Handling**: It allows the caller to register an error handler that will be notified if the remote process disconnects or crashes. This is critical for robustly managing the lifecycle of cross-process connections.

*   **`AcceptInvitation`**: This is the corresponding operation in the remote process. It takes a platform handle (the other end of the channel) and registers it with the `NodeController` to establish the connection.

### Message Handling and Validation

While most message logic resides in `Channel` and `UserMessageImpl`, `Core` provides the top-level API and adds a final layer of validation.

*   **`NotifyBadMessage`**: This API is a critical part of the security model. It allows a process to report that it has received a malformed or unexpected message from a peer. The `Core` routes this notification to the `NodeController`, which can then use its internal knowledge of the peer's identity (`source_node`) to take action, such as terminating the offending process. This provides a formal mechanism for punishing misbehaving IPC partners rather than simply crashing.

## 4. Potential Vulnerabilities & Mitigations

*   **Race Conditions**:
    *   **Threat**: Multiple threads manipulating the same handle could lead to use-after-free or other corruption.
    *   **Mitigation**: The `HandleTable` and `MappingTable` are both protected by locks. All operations on these tables are performed while holding the appropriate lock, providing strong protection against race conditions.

*   **Resource Exhaustion (Denial of Service)**:
    *   **Threat**: A malicious actor could attempt to create an unlimited number of handles or memory mappings, exhausting process resources.
    *   **Mitigation**: The `HandleTable` has an implicit limit based on available memory. The `MappingTable` has an explicit, configurable size limit (`max_mapping_table_size`). Functions that deserialize messages also have internal limits, such as `kMaxHandlesPerMessage`, to prevent absurdly large allocation requests.

*   **Logic Bugs in Invitation Flow**:
    *   **Threat**: The process of creating, sending, and accepting invitations is complex. A logic flaw could allow a process to connect to an unintended peer or bypass security policies.
    *   **Mitigation**: The logic is well-encapsulated. The `InvitationDispatcher` holds the state for a pending invitation, and the `Core` API ensures that handles are correctly created and consumed. The use of flags like `ISOLATED` and `UNTRUSTED_PROCESS` provides clear security annotations for the `NodeController` to act upon.

## 5. Conclusion

The `Core` class is the central nervous system of Mojo IPC. Its design demonstrates a robust security posture built on several key principles: centralized ownership of critical resources (`HandleTable`, `NodeController`), strong typing through the `Dispatcher` pattern, meticulous locking to prevent race conditions, and explicit mechanisms for resource limiting and error handling (`NotifyBadMessage`). While the low-level `Channel` is responsible for safely parsing bytes, `Core` is responsible for safely managing object lifecycles and enforcing the high-level security policy of the entire Mojo system. The invitation workflow remains the most complex and, therefore, highest-risk area for potential logic vulnerabilities.