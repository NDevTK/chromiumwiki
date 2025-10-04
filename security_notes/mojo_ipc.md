# Mojo IPC: Architecture and Security Model

Mojo is Chromium's underlying IPC (Inter-Process Communication) framework. It is the foundation upon which the entire multi-process security architecture is built. Understanding Mojo is critical to understanding how Chromium isolates processes and protects them from each other.

This document is based on an analysis of the Mojo C++ binding source code in `mojo/public/cpp/bindings/`.

## Core Architectural Components

At its heart, Mojo is a message-passing system built around a few key primitives:

1.  **Messages**: The fundamental unit of data transfer. Messages consist of a header and a payload. They are serialized into a defined format for transfer between processes.
2.  **Message Pipes**: A pair of related endpoints that provide a bidirectional channel for sending and receiving messages. A handle to one end of a pipe can be transferred to another process, establishing a direct communication channel.
3.  **Mojom (Mojo Interface Definition Language)**: Similar to Protocol Buffers or AIDL, developers define interfaces in `.mojom` files. The Mojo toolchain uses these files to generate language-specific boilerplate code (e.g., C++ or JavaScript) for serializing and deserializing messages, as well as proxy and stub classes.

## The C++ Bindings: `Remote` and `Receiver`

The generated code and the C++ bindings library provide a high-level, type-safe, and developer-friendly way to interact with Mojo pipes. The two most important classes are `Remote` and `Receiver`.

### `Remote<T>`: The Client-Side Proxy

A `Remote<T>` is a client-side handle to a remote Mojo interface `T`. It acts as a proxy, converting C++ method calls into serialized Mojo messages.

*   **Usage**: To make a call, a developer gets a `Remote` object and calls methods on it as if it were a local object (`remote->MyMethod(args)`). The `Remote` class handles the serialization and sends the message over its bound message pipe.
*   **Binding**: A `Remote` is useless until it is "bound" to a message pipe endpoint. This is typically done by creating a new pipe and passing the receiving end (`PendingReceiver`) to another process, which will then bind it to an implementation.
*   **Lifetime and Security**: The `Remote` class is designed with memory safety in mind. Callbacks for method responses are tied to the lifetime of the `Remote` object. If the `Remote` is destroyed, any pending callbacks are cancelled, preventing use-after-free vulnerabilities where a response might arrive after the calling code has been deallocated.

### `Receiver<T>`: The Server-Side Stub

A `Receiver<T>` is the server-side endpoint that listens for incoming messages, deserializes them, and dispatches them to a local C++ implementation of the interface `T`.

*   **Usage**: A developer creates an object that implements the `T` interface and then constructs a `Receiver`, giving it a pointer to the implementation object. The `Receiver` is then bound to a message pipe endpoint (`PendingReceiver`).
*   **Dispatch**: When a message arrives, the `Receiver` deserializes it, validates it, and invokes the corresponding C++ method on the bound implementation object.
*   **Lifetime and Security**: Like the `Remote`, the `Receiver` is designed for memory safety. If the `Receiver` is destroyed, it will no longer dispatch any queued method calls, even if they have already arrived. This prevents calls from being dispatched to a destroyed implementation object.

## Mojo's Multi-Layered Security Model

Security is a primary design goal of Mojo. It achieves this through a multi-layered validation approach, assuming that any message from another process could be malicious or malformed.

1.  **Layer 1: Header Validation (`MessageHeaderValidator`)**
    *   This is the first line of defense. Every message that arrives at a `Receiver` is first passed through a `MessageHeaderValidator`.
    *   This validator checks the structural integrity of the message header itself. It ensures the header size is correct, flags are valid, and the message size doesn't exceed reasonable limits. This prevents basic exploits like buffer overflows caused by a malformed header.

2.  **Layer 2: Payload Validation (Generated Code)**
    *   After the header is deemed valid, the message payload is deserialized by the auto-generated binding code.
    *   This code contains its own validation logic. It checks that the payload size matches what is expected for the given method, that pointers within the payload are valid, that enums have defined values, and that handle counts are correct. This ensures the message payload conforms to the `.mojom` definition.

3.  **Layer 3: Semantic Validation (`ReportBadMessage`)**
    *   This is the final and most application-specific layer of validation. The `Receiver` provides a `ReportBadMessage()` method to the C++ implementation.
    *   If the implementation, after receiving a structurally and syntactically valid message, determines that its *content* is semantically invalid or malicious (e.g., an out-of-bounds index, an unexpected value), it can call `ReportBadMessage()`.
    *   This action immediately tears down the Mojo connection. In most cases, it also results in the **termination of the sending process**. This is the ultimate defense against a compromised renderer or other sandboxed process attempting to exploit a logical bug in a browser-process service.

This layered approach ensures that messages are rigorously validated at multiple stages, making Mojo a robust foundation for building a secure, multi-process application like Chromium.