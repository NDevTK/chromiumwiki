# Mojo Broker (`mojo/core/broker.h`)

## 1. Summary

The Mojo `Broker` is a small but critical component in Chromium's multi-process security architecture. It provides a synchronous IPC mechanism for a less-privileged process (like a sandboxed renderer) to request a specific, privileged operation from a more-privileged process (the "broker process," which is typically the main browser process).

Its primary and most security-sensitive function is to broker the creation of **shared memory regions**. Sandboxed processes are often forbidden from creating shared memory directly, as this could be abused. The Mojo Broker acts as a trusted intermediary, allowing the sandboxed process to request a shared memory segment, which the broker creates and then passes back a handle to.

## 2. Core Concepts

*   **Privilege Separation & Delegation:** The entire model is based on privilege separation. The client of the `Broker` class is a low-privilege process, while the process hosting the other end of the Mojo pipe is a high-privilege one. The low-privilege process delegates the security-sensitive operation (shared memory creation) to the high-privilege process.

*   **Synchronous IPC:** Unlike most Mojo communication, which is asynchronous, the broker channel is synchronous. This is a deliberate design choice. When a process needs a shared memory region, it often cannot proceed until it has it, so it must block and wait for the broker to create it and return the handle. The `lock_` member ensures that these synchronous requests are serialized, preventing races.

*   **Bootstrapping Mojo Connections:** The `Broker` also plays a role in bootstrapping a new process into an existing Mojo network. When a new process is launched, it can be given a handle to a broker. Its first action is to read a message from the broker channel which contains the `PlatformChannelEndpoint` for the main Mojo `NodeChannel`. This is how a new process gets the information it needs to connect to its parent and start receiving IPCs.

## 3. Security-Critical Logic & Vulnerabilities

The `Broker` client class itself is simple, but the security of the overall pattern depends entirely on the implementation and integrity of the **broker process** on the other end of the pipe.

*   **The Broker Process as a Target:** The broker process is a highly privileged target. If an attacker could find a vulnerability in the broker itself, they could potentially compromise it and use its privileges to escape the sandbox. Therefore, the broker's attack surface must be kept as minimal as possible.

*   **Request Validation:** The most critical vulnerability vector would be a lack of validation in the broker process.
    *   When handling `GetWritableSharedMemoryRegion`, the broker **must** validate the requested `num_bytes`. If it doesn't, a malicious renderer could request an enormous region of shared memory, leading to resource exhaustion and a denial-of-service attack on the entire system.
    *   The broker must also apply the correct, restrictive permissions to any handles it creates before passing them back to the less-privileged process.

*   **Bootstrapping Integrity:** A flaw in the initial handle-passing mechanism (`GetInviterEndpoint`) could be catastrophic. If an attacker could trick the broker into providing a handle to a different Mojo network (e.g., one belonging to a different `SiteInstance` or a privileged extension process), it could allow the compromised process to intercept and send IPCs it should not have access to.

*   **Denial of Service:** Even with size validation, a malicious process could repeatedly make valid requests through the broker in a tight loop, consuming CPU and IPC resources in the browser process. The broker may need rate-limiting or other heuristics to detect and mitigate such abuse.

## 4. Key Functions

*   `Broker(PlatformHandle handle, bool wait_for_channel_handle)`: The constructor. The `wait_for_channel_handle` parameter is security-critical, as it determines whether this broker is being used for bootstrapping a new Mojo connection.
*   `GetWritableSharedMemoryRegion(size_t num_bytes)`: The primary method for requesting a privileged operation. The security of this call depends on the validation performed by the broker process on the other side.
*   `GetInviterEndpoint()`: The method used during process startup to get the handle to the main Mojo channel, allowing the new process to join the Mojo network.

## 5. Related Files

*   `mojo/core/broker_host.h`: The server-side implementation that runs in the browser process. This class receives requests from the `Broker` client and is responsible for all the critical validation.
*   `content/browser/child_process_launcher.cc`: This is typically where a new process is created and where the initial broker handle is passed to it, initiating the secure bootstrapping process.
*   `base/memory/writable_shared_memory_region.h`: The data structure representing the shared memory region that is created by the broker and returned to the client.