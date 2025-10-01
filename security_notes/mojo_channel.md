# Mojo Channel (`mojo/core/channel.h`)

## 1. Summary

The `Channel` class is the foundational component of the Mojo IPC system. It provides a low-level, platform-agnostic, and thread-safe mechanism for sending and receiving delimited messages between two processes. It operates directly on top of a platform-specific transport (e.g., a Unix domain socket or a Windows named pipe) and is responsible for message serialization, deserialization, and the secure transfer of native platform handles.

The correctness and security of the `Channel` are the bedrock upon which Chromium's entire multi-process security model is built. A vulnerability at this layer, such as a flaw in message parsing, could undermine all higher-level security policies and potentially allow a sandboxed process to compromise the browser.

## 2. Core Concepts

*   **Low-Level Transport:** The `Channel` is an abstraction over a raw I/O handle. Its platform-specific implementations (`ChannelPosix`, `ChannelWin`, etc.) handle the details of reading from and writing to the underlying OS pipe.

*   **Message Framing:** Mojo messages are not just raw streams of bytes; they are framed. The `Channel::Message` struct defines a header (`Header` or `LegacyHeader`) that is prepended to every message. This header is critical as it contains the total size of the message (`num_bytes`) and the number of handles attached (`num_handles`). This allows the receiving end to know exactly how many bytes to read to get one complete, self-contained message.

*   **Platform Handle Transfer:** A core feature of Mojo is the ability to securely transfer native OS handles (e.g., file descriptors, shared memory handles) between processes. The `Channel` is responsible for the low-level mechanics of this, working with the OS to send and receive these handles as part of a message.

*   **Delegate Model:** The `Channel` itself is only concerned with transport. It does not understand the *meaning* of the messages it carries. When a valid message is successfully received and deserialized, the `Channel` passes the payload and any attached handles up to a `Delegate` (typically the `NodeController`), which is responsible for interpreting the message and dispatching it to the correct higher-level Mojo endpoint.

## 3. Security-Critical Logic & Vulnerabilities

The `Channel` operates at the ultimate trust boundary, receiving raw bytes from a potentially malicious process. Its security is therefore paramount.

*   **Message Deserialization (`Message::Deserialize`):** This is the single most security-critical function. It is responsible for taking a buffer of untrusted bytes and parsing it into a valid `Message` object.
    *   **Risk:** An attacker could craft a malicious message with incorrect header values. An integer overflow in the `num_bytes` field could cause the `Channel` to attempt to read a massive amount of data, leading to a crash or out-of-bounds read. An incorrect `num_header_bytes` or `num_handles` could lead to the parser misinterpreting the message structure, potentially treating part of the payload as a handle or vice-versa.
    *   **Mitigation:** This function must be completely robust against malformed input. It must perform rigorous checks on all header fields (e.g., that `num_header_bytes` is less than `num_bytes`) before attempting to process the message. Any failure must result in the message being rejected and the channel being closed.

*   **Handle Policy Enforcement:**
    *   **Risk:** Some Mojo connections are not intended to transfer privileged platform handles. The `Channel` is constructed with a `HandlePolicy` (`kAcceptHandles` or `kRejectHandles`). If a channel configured with `kRejectHandles` were to incorrectly process a message containing handles, it could allow a compromised process to inject a privileged capability (like a writable file handle) into a process that is not equipped to handle it securely.
    *   **Mitigation:** The `Message::Deserialize` logic must check the `handle_policy_` of the `Channel` and immediately reject any message containing handles if the policy is `kRejectHandles`.

*   **Resource Exhaustion (DoS):**
    *   **Risk:** A malicious process could flood the channel with a huge number of valid but useless messages, consuming CPU and memory in the target process and leading to a denial of service.
    *   **Mitigation:** While the `Channel` itself has limited defenses against this, the higher-level Mojo infrastructure (`NodeController`) has mechanisms for throttling and managing message queues. The `Channel`'s role is to ensure that processing a single malformed message cannot, by itself, lead to unbounded resource consumption.

## 4. Key Functions

*   `Channel::Create(...)`: The static factory method that creates a platform-specific `Channel` implementation around a native OS pipe.
*   `Channel::Write(MessagePtr)`: The entry point for queuing a serialized message to be sent over the pipe.
*   `Delegate::OnChannelMessage(...)`: The callback invoked by the `Channel` after it has successfully received and deserialized a complete, valid message.
*   `Message::Deserialize(...)`: The critical parsing and validation function that turns raw bytes from the pipe into a structured `Message` object.
*   `OnError(Error error)`: The function called to shut down the channel when an unrecoverable error (like receiving malformed data) occurs.

## 5. Related Files

*   `mojo/core/node_controller.h`: The primary delegate for the `Channel`. It receives validated messages and routes them to the appropriate message pipes within the process.
*   `mojo/core/channel_posix.cc`, `mojo/core/channel_win.cc`: The platform-specific implementations that handle the actual socket I/O.
*   `mojo/public/cpp/platform/platform_handle.h`: Defines the platform-agnostic wrapper for native OS handles that are transferred by the `Channel`.