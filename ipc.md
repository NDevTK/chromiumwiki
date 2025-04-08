# Inter-Process Communication (IPC) in Chromium: Security Considerations

This page documents potential security vulnerabilities related to inter-process communication (IPC) in Chromium, focusing on legacy IPC (`ipc/`) and Mojo (`mojo/`). IPC is crucial for communication between different browser processes (browser, renderer, GPU, utility, etc.), and vulnerabilities here could allow attackers to bypass process boundaries (sandbox escape), inject malicious code, gain unauthorized capabilities, or cause denial-of-service conditions.

## Potential Vulnerabilities (General IPC/Mojo):

*   **Insufficient Validation/Access Control:** Browser-process handlers failing to validate data or check permissions for messages received from less privileged processes (renderers, utility processes).
    *   **VRP Pattern (Missing Origin/Permission Checks):** Allowing actions on behalf of incorrect origins (e.g., Push Messaging VRP: `1275626`; Content Index VRP: `1263530`, `1263528`). Extensions accessing APIs without proper checks (VRP2.txt#11815).
    *   **VRP Pattern (Capability Bypass via Unvalidated Metadata):** Allowing less privileged processes to trigger privileged actions by sending valid *data* but potentially unvalidated *metadata*. For example, the `StartDragging` IPC handler (`RenderWidgetHostImpl::StartDragging`) filters dragged file paths based on renderer permissions but might not sufficiently validate associated metadata like the starting screen location or allowed operations mask before passing them to platform APIs (`WebContentsView*::StartDragging` -> `aura::client::DragDropClient::StartDragAndDrop`). This could allow a compromised renderer to gain unintended control over the mouse cursor or trigger drag operations beyond its permissions (VRP2.txt#4). See [drag_and_drop.md](drag_and_drop.md).
    *   **VRP Pattern (Incorrect Destination Handling):** Browser process mishandling messages intended for a different process (e.g., Browser handling renderer `ACCEPT_BROKER_CLIENT` message VRP2.txt#370).
*   **Memory Safety (UAF, Buffer Overflows, Integer Overflows, Type Confusion):** Classic memory safety bugs within the IPC message handling code itself, or in the code processing the received data, especially when parsing complex data structures or handling large messages. Type confusion in Mojo interfaces (VRP: `1337607`).
*   **Race Conditions & State Confusion:** Asynchronous nature of IPC leading to race conditions, dangling pointers (e.g., `PermissionRequestManager` UAF VRP: `1424437`), or inconsistent state management across processes. Use-after-free triggered via IPC through side panel feature (VRP: `40061678`). Double fetch via code cache IPC (VRP2.txt#542).
*   **Injection Attacks:** Insufficient sanitization or validation of data received via IPC leading to injection vulnerabilities (though less common than direct memory corruption).
*   **Process Launching and Sandboxing:** Incorrect sandboxing of child processes launched via IPC (e.g., `Launch` in `BrowserChildProcessHostImpl`) could allow a compromised child process broader system access. Exploiting COM interfaces on Windows for process launching (`GoogleUpdate.ProcessLauncher` + Session Moniker VRP: `340090047`, VRP2.txt#3763).
*   **Mojo Specific Issues:** Vulnerabilities related to Mojo features like shared buffers, handles, or interface brokering. Insecure handling of special Mojo types like `native_struct.mojom`. Unsafe deserialization.
*   **Information Leaks:** IPC mechanisms inadvertently leaking sensitive data between processes.

### Legacy IPC (`ipc/`) Specific Considerations:
*   Primarily used for browser↔renderer communication before widespread Mojo adoption.
*   Uses macros (`IPC_MESSAGE_HANDLER`, `IPC_MESSAGE_FORWARD`) for message routing. Review these handlers for validation issues.
*   Serialization uses `ParamTraits`; review `Write` and `Read` methods for memory safety bugs.

### Mojo (`mojo/`) Specific Considerations:
*   Modern IPC framework based on defined interfaces (`.mojom` files).
*   Interfaces can be defined between various process types (browser, renderer, GPU, utility, services).
*   Focus on validation within the browser-process implementation of interfaces exposed to less privileged processes. Check for `ReportBadMessage` calls, which often indicate security checks.
*   Analyze interactions with interface brokers (`BrowserInterfaceBroker`, `RenderFrameHost::GetAssociatedInterface`, etc.) for potential security issues.

## Further Analysis and Potential Issues:

* **Input Validation:** All IPC/Mojo messages received in a more privileged process from a less privileged one (especially browser process from renderer) *must* be rigorously validated. Assume the sender is malicious. Validate origins, permissions, *metadata* (e.g., flags, geometry, operations masks), and *data* (e.g., URLs, file paths, serialized objects) based on context and permissions.
* **Synchronization:** Explicitly handle potential race conditions using appropriate synchronization primitives when IPC handlers access shared state.
* **Authorization:** Verify caller identity and permissions before performing sensitive actions triggered via IPC. Use mechanisms like `RenderFrameHost::GetLastCommittedOrigin` and associated security policy checks (`ChildProcessSecurityPolicy`).
* **Error Handling:** Implement robust error handling for IPC failures (`OnMojoError`, etc.) without revealing sensitive information. Disconnect or terminate misbehaving processes (`ReportBadMessage`).
* **Child Process Management:** Ensure secure lifecycle management (launching, connection, termination, resource cleanup) for all child process types.

## Areas Requiring Further Investigation:

* Review all Mojo interface implementations in privileged processes (browser, network, GPU) that are exposed to less privileged processes (renderer, utility). Pay special attention to interfaces handling input, file access, permissions, device access, navigation, and rendering commands, checking validation of *all* parameters.
* Audit legacy IPC handlers (`OnMessageReceived` in `RenderProcessHostImpl`, `RenderFrameHostImpl`, etc.) for similar validation issues.
* Analyze the security of Interface Brokers and how interfaces are obtained across process boundaries.
* Review `ParamTraits` implementations for legacy IPC for memory safety.
* Fuzz Mojo interfaces exposed to renderers.

## Files Reviewed:
* `content/browser/browser_child_process_host_impl.cc` (Previously focused, but IPC is broader)
* `content/browser/renderer_host/render_widget_host_impl.cc` (Handles `StartDragging` IPC)

## Key Functions/Concepts Reviewed:
* `OnMessageReceived`, `ReportBadMessage`, `OnMojoError`
* Mojo Interface Definitions (`.mojom`) and Implementations
* `RenderWidgetHostImpl::StartDragging` (Example IPC handler with potential metadata validation issues)
* Interface Brokers (`BrowserInterfaceBroker`)
* `ChildProcessSecurityPolicy`
