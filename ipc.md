# Inter-Process Communication (IPC) in Chromium: Security Considerations

This page documents potential security vulnerabilities related to inter-process communication (IPC) in Chromium, focusing on legacy IPC (`ipc/`) and Mojo (`mojo/`). IPC is crucial for communication between different browser processes (browser, renderer, GPU, utility, etc.), and vulnerabilities here could allow attackers to bypass process boundaries (sandbox escape), inject malicious code, gain unauthorized capabilities, or cause denial-of-service conditions.

## Potential Vulnerabilities (General IPC/Mojo):

*   **Insufficient Validation/Access Control:** Browser-process handlers failing to validate data or check permissions for messages received from less privileged processes (renderers, utility processes).
    *   **VRP Pattern (Missing Origin/Permission Checks):** Allowing actions on behalf of incorrect origins (e.g., Push Messaging VRP: `1275626`; Content Index VRP: `1263530`, `1263528`). Extensions accessing APIs without proper checks (VRP2.txt#11815).
    * **VRP Pattern (Capability Bypass via Unvalidated Metadata):** Allowing less privileged processes to trigger privileged actions by sending valid *data* but potentially unvalidated *metadata* received via IPC. A compromised renderer could send crafted metadata. For instance, in `RenderWidgetHostImpl::StartDragging` (VRP2.txt#4), while file paths in the `DropData` are filtered based on renderer permissions, the `drag_operations_mask` and `event_info` (containing screen location) are passed directly to the delegate view:
        ```cpp
        // content/browser/renderer_host/render_widget_host_impl.cc
        void RenderWidgetHostImpl::StartDragging(
            /* ... */, DragOperationsMask drag_operations_mask, /* ... */,
            blink::mojom::DragEventSourceInfoPtr event_info) {
          // ... DropData filtering happens here ...

          RenderViewHostDelegateView* view = delegate_->GetDelegateView();
          // ... checks ...

          // Filtered data, but original mask and event_info are passed down
          view->StartDragging(filtered_data, source_origin, drag_operations_mask, image,
                              offset, rect, *event_info, this);
        }
        ```
        If the platform client (e.g., `ash::DragDropController`) improperly uses this unvalidated metadata during its nested drag loop or OS interactions (rather than relying solely on trusted, real-time events or delegate responses), it could potentially lead to unintended UI control. See [drag_and_drop.md](drag_and_drop.md).
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
*   Focus on validation within the browser-process implementation of interfaces exposed to less privileged processes. Check for `ReportBadMessage` calls, which often indicate security checks. **`ReportBadMessage` is widely used in privileged process handlers (e.g., in `content/browser/renderer_host/`) to terminate renderers sending invalid messages, enforcing checks on feature flags, frame state/context, origins, parameters, and API-specific rules. Its heavy usage in areas like Fenced Frames indicates complex security boundaries.**
*   Analyze interactions with interface brokers (`BrowserInterfaceBroker`, `RenderFrameHost::GetAssociatedInterface`, etc.) for potential security issues.

## Further Analysis and Potential Issues:

* **Input Validation:** All IPC/Mojo messages *and responses* received in a more privileged process from a less privileged one (especially browser process from renderer) *must* be rigorously validated. Assume the sender is malicious. Validate origins, permissions, *metadata* (e.g., flags, geometry, operations masks, screen locations), and *data* (e.g., URLs, file paths, serialized objects, chosen operations) based on context and permissions. Re-validate crucial parameters in the browser process even if checks exist in the renderer.
* **Synchronization:** Explicitly handle potential race conditions using appropriate synchronization primitives when IPC handlers access shared state.
* **Authorization:** Verify caller identity and permissions before performing sensitive actions triggered via IPC. Use mechanisms like `RenderFrameHost::GetLastCommittedOrigin` and associated security policy checks (`ChildProcessSecurityPolicy`).
* **Error Handling:** Implement robust error handling for IPC failures (`OnMojoError`, etc.) without revealing sensitive information. Disconnect or terminate misbehaving processes (`ReportBadMessage`).
* **Child Process Management:** Ensure secure lifecycle management (launching, connection, termination, resource cleanup) for all child process types.

## Areas Requiring Further Investigation:

*   **Mojo Interface Validation (Privileged Processes):**
    *   Systematically review implementations in the browser process (and other privileged processes like Network, GPU) of Mojo interfaces exposed to less privileged processes (renderer, utility).
    *   **Focus on Origin/Permission Checks:** Pay special attention to interfaces handling features implicated in VRPs (Push Messaging VRP `1275626` - check handlers in `content/browser/push_messaging/`; Content Index VRP `1263530`/`1263528` - check handlers in `content/browser/content_index/`; Extensions VRP2.txt#11815 - check extension API handlers). Verify handlers robustly check the sender's origin (`RenderFrameHost::GetLastCommittedOrigin`) and permissions (`ChildProcessSecurityPolicy`) before acting.
    *   **Focus on Metadata Validation:** Audit handlers receiving complex data structures or parameters beyond simple URLs/origins. Ensure *metadata* (e.g., screen coordinates, flags, operation masks in `RenderWidgetHostImpl::StartDragging` - VRP2.txt#4) is validated or treated as untrusted before use in privileged operations or platform APIs.
    *   **Look for `ReportBadMessage` calls** as indicators of existing security checks; ensure checks are comprehensive and cover all relevant parameters. Note its heavy use for Fenced Frame validation.
*   **Race Conditions & State Management (UAF):**
    *   Audit component lifecycles interacting heavily with asynchronous IPCs.
    *   Specifically review `PermissionRequestManager` (`components/permissions/`) state management related to request cancellation/finalization IPCs (VRP `1424437`). Are there race conditions between IPC arrival and internal state changes/object destruction?
    *   Review Side Panel (`chrome/browser/ui/views/side_panel/` or related components) IPC handlers and state management for potential UAFs during asynchronous operations (VRP `40061678`).
*   **Memory Safety (Legacy IPC & Mojo):**
    *   Audit legacy IPC handlers (`OnMessageReceived` in `RenderProcessHostImpl`, `RenderFrameHostImpl`, etc.) for validation and memory safety issues.
    *   Review `ParamTraits` implementations for legacy IPC serialization (`Write`, `Read`) for memory safety bugs (overflows, etc.).
    *   Audit Mojo struct deserialization (`StructTraits::Read`) and C++ code processing complex IPC data structures for type confusion (VRP `1337607`), buffer overflows, integer overflows etc.
*   **Process Launching & OS Interaction (EoP):**
    *   Review browser process interactions with platform-specific process launching mechanisms, especially external COM interfaces on Windows (`GoogleUpdate.ProcessLauncher`). Ensure proper validation, sandboxing, and impersonation levels are used (VRP2.txt#3763). Audit `BrowserChildProcessHostImpl::Launch`.
*   **Interface Broker Security:** Analyze the security of Interface Brokers (`BrowserInterfaceBroker`, `RenderFrameHost::GetAssociatedInterface`) and how interfaces are obtained/exposed across process boundaries. Can a renderer obtain an interface it shouldn't have access to?
*   **Fuzzing:** Fuzz Mojo interfaces exposed to renderers, focusing on complex data structures and edge case inputs.

## Files Reviewed:
* `content/browser/browser_child_process_host_impl.cc`
* `content/browser/renderer_host/render_widget_host_impl.cc`

## Key Functions/Concepts Reviewed:
* `OnMessageReceived`, `ReportBadMessage`, `OnMojoError`
* Mojo Interface Definitions (`.mojom`) and Implementations
* `RenderWidgetHostImpl::StartDragging` (Example IPC handler with potential metadata validation issues - VRP2.txt#4)
*   `RenderWidgetHostImpl::DragTarget*` methods: Browser->Renderer IPC for drag events (Enter, Over, Leave, Drop).
*   `RenderWidgetHostImpl::OnUpdateDragOperation`: Browser receives chosen drop operation from Renderer via IPC.
* Interface Brokers (`BrowserInterfaceBroker`)
* `ChildProcessSecurityPolicy`

## Cross-References
*   [mojo.md](mojo.md)
*   [drag_and_drop.md](drag_and_drop.md)
*   [permissions.md](permissions.md) (Related UAF VRP)
*   [side_panel.md](side_panel.md) (Related UAF VRP)
*   [extension_security.md](extension_security.md) (IPC Spoofing)
*   [installer_security.md](installer_security.md) (COM EoP)
