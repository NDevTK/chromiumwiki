# Security Analysis of `mojo::Receiver`

This document analyzes the security properties of `mojo::Receiver`, as defined in `mojo/public/cpp/bindings/receiver.h`. The `Receiver` is the fundamental class for binding a C++ implementation of a mojom interface to a message pipe. It is the non-associated counterpart to `AssociatedReceiver`.

## Overview

The `Receiver` class is responsible for receiving messages from a `Remote`, deserializing them, and dispatching them as method calls to a local C++ object that implements the mojom interface.

### Key Components:

-   **`Receiver<Interface, ImplRefTraits>`**: The main, user-facing class template. It provides a high-level API for binding, unbinding, and configuring the receiver.
-   **`internal::BindingState`**: The internal class that holds the core logic of a receiver. It is owned by the `Receiver`.

## Internal Architecture

A `Receiver`'s logic is almost entirely delegated to an `internal::BindingState` object. This internal class is responsible for creating and managing the core Mojo bindings machinery, including the `MultiplexRouter` and `InterfaceEndpointClient`.

For a detailed analysis of these internal components and their interactions, see **`mojo_internals.md`**.

## Security-Relevant Observations

Many of the security considerations for `Receiver` are similar to those for `AssociatedReceiver`, but there are some key differences due to its non-associated nature.

### 1. Lifetime and Implementation Ownership

-   **Raw Pointer to Implementation**: `Receiver` defaults to using a raw pointer (`ImplPointerType`) to the interface implementation. This creates a risk of use-after-free if the implementation object is destroyed before the `Receiver` that points to it. The `ImplRefTraits` parameter allows for safer pointer types, but the default behavior is a potential source of vulnerabilities.
-   **No Passive Unbinding**: The documentation explicitly states that a `Receiver` is never passively unbound. It only becomes unbound when `reset()` or `Unbind()` is called. This means that its internal machinery will continue to watch the pipe and dispatch messages as long as it is alive and bound, making the lifetime management of the implementation critical.

### 2. Error and State Management

-   **Disconnection Handlers**: `set_disconnect_handler()` and `set_disconnect_with_reason_handler()` are the only reliable way to be notified of a connection error. Failing to set a handler can lead to a silent failure mode where the application state becomes inconsistent.
-   **Bad Message Reporting**: The `ReportBadMessage()` and `GetBadMessageCallback()` methods are the correct way to handle malformed messages. When a bad message is reported, the underlying connection is closed, which is a safe and robust way to handle potential attacks.
-   **Pause/Resume**: The `Pause()` and `Resume()` methods allow the user to temporarily stop and start the dispatch of incoming messages. This could be a source of subtle bugs if not used carefully. For example, if a receiver is paused for too long, it could cause the message pipe to fill up, leading to a denial-of-service.
-   **`Unbind()` and Associated Interfaces**: The `Unbind()` method `CHECK`s that there are no active associated interfaces. This is a critical safety check that prevents a `Receiver` from being unbound while it is still the primary pipe for other associated interfaces, which would leave them in a dangling, unusable state.

### 3. Synchronous Operations

-   **`WaitForIncomingCall()`**: This method blocks the calling thread until a message is received and dispatched. This is a potential source of deadlocks if not used carefully. For example, if two processes are both waiting for each other to send a message, they will deadlock. This is also a potential DoS vector, as a malicious remote could simply never send a message, causing the thread to block indefinitely.

### 4. Thread Safety

The `Receiver` class is explicitly **not thread-safe**. It must be created, used, and destroyed on a single `base::SequencedTaskRunner`. This sequence-affinity is inherited from its internal components. This design simplifies the internal logic but places the burden of correct thread usage on the developer.

## Summary of Security Posture

The `Receiver` class is the workhorse of the Mojo C++ bindings. Its security posture is generally robust, but it has several sharp edges that developers must be aware of.

-   **Lifetime Management**: The default use of raw pointers is a major potential source of use-after-free vulnerabilities.
-   **Error Handling**: Proper use of disconnection handlers and bad message reporting is essential for robust error handling.
-   **Synchronous Calls**: `WaitForIncomingCall()` can lead to deadlocks and DoS vulnerabilities if misused.
-   **Associated Interfaces**: The `CHECK` in `Unbind()` highlights the complexity of managing associated interfaces and the potential for errors if the `Receiver` is unbound prematurely.

A security review of code using `Receiver` should focus on verifying correct lifetime management of the implementation object, proper use of error handlers, and avoidance of risky synchronous operations. For a deeper understanding of the underlying mechanics, refer to `mojo_internals.md`.