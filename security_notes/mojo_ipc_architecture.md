# Security Architecture of the Mojo IPC System

## 1. Overview: The Foundation of Inter-Process Communication

Mojo is Chromium's modern Inter-Process Communication (IPC) system. It is the bedrock upon which the entire multi-process security architecture is built. Every request that crosses a sandbox boundary—from a renderer asking to draw pixels, to a network request, to a GPU command—is transported over Mojo. Its primary security goal is to provide a safe, reliable, and performant communication channel between processes of different privilege levels.

A vulnerability in the low-level Mojo implementation could be catastrophic, as it would likely be exploitable from any sandboxed process, leading to a full sandbox escape and compromise of the browser. Therefore, its design and implementation are among the most security-critical in the entire Chromium codebase.

## 2. Core Architectural Components

Mojo's architecture is layered, with high-level, easy-to-use bindings built on top of a robust and secure low-level core.

### The Public API and Bindings (`//mojo/public`)

*   **Mojom IDL**: This is the Interface Definition Language for Mojo. Developers define their IPC interfaces in `.mojom` files. The Mojom compiler then generates C++, Java, and JavaScript code for serializing and deserializing messages according to that interface.
*   **Bindings (`mojo::Remote`, `mojo::Receiver`)**: These are the high-level C++ classes that developers use. They provide a type-safe, easy-to-use API that feels like calling a local C++ object, but which under the hood is sending and receiving IPC messages.
*   **Security Implication**: The Mojom IDL and the bindings are the **first line of defense**. The generated code includes validation checks to ensure that received messages conform to the interface definition (e.g., non-nullable fields are not null, enums have valid values). A message that fails this validation is rejected, and the pipe is disconnected. This prevents malformed data from reaching the application-level IPC handler.

### Mojo Core (`//mojo/core`)

This is the heart of the Mojo system, responsible for the low-level mechanics of IPC.

*   **`Core` (`core.cc`)**: The process-global singleton that manages all Mojo state. It holds the crucial `HandleTable`, which maps opaque `MojoHandle` integers to the internal `Dispatcher` objects that implement their functionality. This is the ultimate gatekeeper for all Mojo API calls.

*   **`Channel` (`channel.cc`)**: This is the low-level transport layer. It is responsible for serializing messages and handles into a byte buffer and writing it to a platform-specific transport (e.g., a socket or named pipe). Its most security-critical role is on the receiving end, where it must safely parse raw, untrusted byte streams from other processes. It performs rigorous validation on message headers, sizes, and handle counts to prevent memory corruption.

*   **`NodeController` & `ports`**: This is the abstract routing layer of Mojo. Every process is a "node" in a graph. The `NodeController` is responsible for routing messages between nodes, whether they are in the same process or different processes. It manages the lifecycle of connections to other processes.

*   **`Broker` (`broker.h`, `broker_host.h`)**: A specialized, synchronous IPC mechanism used for specific, privileged operations. Its primary use is to allow a sandboxed process to request the creation of a shared memory region from the privileged browser process. The broker process must rigorously validate all requests from the client to prevent resource exhaustion attacks.

## 3. The IPC Bootstrapping Process: A Security-Critical Handshake

Establishing a Mojo connection between the browser and a new sandboxed process is a critical security handshake.

1.  **Launch**: The browser process (the "inviter") launches a new target process (the "invitee").
2.  **Invitation**: The browser creates a Mojo "invitation". It attaches one end of a new message pipe to this invitation.
3.  **Handle Passing**: The browser uses a platform-specific mechanism (e.g., command-line arguments on POSIX, `DuplicateHandle` on Windows) to pass the other end of the invitation's transport channel (e.g., a socket) to the new target process.
4.  **Acceptance**: The target process starts, receives the handle, and calls `MojoAcceptInvitation`. This connects it back to the browser's `NodeController`.
5.  **Pipe Extraction**: The target process can now extract its end of the message pipe that was attached in step 2.
6.  **Communication**: The two processes now have a fully functional, sandboxed IPC channel.

The security of this process relies on the OS ensuring that only the intended target process can receive and accept the initial handle.

## 4. Security Posture and Conclusion

Mojo's security is built on a defense-in-depth strategy:

1.  **The Sandbox**: Mojo assumes it is communicating with untrusted, potentially malicious processes. It does not trust the sender to be well-behaved.
2.  **Mojom Validation**: The automatically generated bindings provide strong type checking and structural validation of all messages. This is the first and most important gate.
3.  **`Channel` Deserialization**: The low-level `Channel` performs rigorous validation on the raw byte stream, checking sizes, handle counts, and memory alignment to prevent memory corruption vulnerabilities. This code is heavily fuzzed.
4.  **`Core` and `HandleTable`**: The `Core` singleton provides strict, centralized management of all Mojo objects and their lifecycles, preventing use-after-free and type confusion bugs at the API level.
5.  **The Broker Pattern**: For the most sensitive operations (like shared memory creation), Mojo uses the broker pattern to delegate the action to a privileged process, minimizing the capabilities granted to sandboxed processes.

Mojo is a complex system, but its layered design and focus on validation at each layer make it a robust foundation for Chromium's security. The most critical areas for security analysis are the low-level `Channel` deserialization code and the logic within the privileged `BrokerHost` for validating requests from untrusted clients.