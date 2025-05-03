# Component: Mojo IPC Framework

## 1. Component Focus
*   **Functionality:** Mojo is Chromium's primary Inter-Process Communication (IPC) system. It facilitates communication between different process types (Browser, Renderer, GPU, Utility, etc.) using message pipes and defined interfaces (`.mojom` files). It handles message serialization/deserialization, routing, and interface binding. Includes core components like `NodeController`, various dispatchers (e.g., `SharedBufferDispatcher`), and bindings for different contexts (e.g., `MojoJS` for WebUI).
*   **Key Logic:** Interface definition (Mojom), message pipe creation/management, endpoint tracking (`NodeController`), message dispatch and handling, type validation (primarily by generated bindings code), proxy/stub generation, security context tracking (implicitly, via process connections and interface routing). **Crucially, security logic (permissions, origin checks, semantic validation) must be implemented manually in the C++ interface implementations.**
*   **Core Files:**
    *   `mojo/core/` (Core implementation, e.g., `node_controller.cc`, `channel.cc`, dispatchers)
    *   `services/network/` (Many network services use Mojo, e.g., `mojo_host_resolver_impl.cc`)
    *   `components/services/`
    *   `content/browser/` & `content/renderer/` (Extensive use for Browser-Renderer communication)
    *   Generated binding files (`//gen/mojo/public/js/bindings.js`, etc.)
    *   Specific interface implementations across various components (e.g., `content/browser/background_fetch/background_fetch_service_impl.cc`, `content/browser/content_index/content_index_database.cc`, `content/browser/push_messaging/push_messaging_manager.cc`, `content/browser/fenced_frame/fenced_frame.cc`, `components/permissions/permission_controller_impl.cc`)

## 2. Potential Logic Flaws & VRP Relevance

Mojo serves as the communication backbone, making its security critical. Vulnerabilities often stem from trusting messages from less privileged processes without sufficient validation in the receiving (often more privileged, typically Browser) process.

*   **Insufficient Validation / Access Control (High Likelihood, High VRP Impact):** Failure to properly validate message parameters, sender origin/privileges, or requested actions in the privileged process implementing the Mojo interface. **This is the most common source of Mojo vulnerabilities.**
    *   **VRP Pattern (Missing/Insufficient Origin Checks):** Browser-side handlers for Mojo interfaces callable from renderers failing to verify the *origin* associated with the request, sometimes only checking less reliable identifiers like `service_worker_registration_id`, or having bypassable checks. Allows a compromised renderer to access/modify data belonging to other origins.
        *   `PushMessaging`: (`Subscribe`, `Unsubscribe`, `GetSubscription`) - Historically leaked subscription info cross-origin (VRP: `1275626`), potentially due to insufficient validation or bypasses of origin checks within the handling logic (Note: `PushMessagingManager` currently uses `ChildProcessSecurityPolicy` checks). See [push_messaging.md](push_messaging.md).
        *   `ContentIndexService`: (`GetDescriptions`, `Delete`) - Leaked/deleted index entries cross-origin (VRP: `1263530`, `1263528`). See [content_index.md](content_index.md).
        *   `BackgroundFetchService`: (`GetDeveloperIds`) - Leaked developer IDs cross-origin (VRP2.txt#14587). See [background_fetch.md](background_fetch.md).
    *   **VRP Pattern (Lack of Browser-Side Capability/Feature Validation):** Browser process handlers trusting renderer-provided information or performing actions without re-validating security prerequisites (e.g., feature flags enabled, permissions granted).
        *   `FencedFrame::CreateFencedFrame`: Browser creating frame without checking `kFencedFrames` feature flag, allowing compromised renderer bypass (VRP2.txt#225). See [fenced_frames.md](fenced_frames.md).
        *   `FencedFrame::Navigate`: Browser process navigating fenced frame to renderer-supplied URL without calling `FilterURL` (allowing `file:` or `chrome:` schemes) (VRP2.txt#225). See [fenced_frames.md](fenced_frames.md).
    *   **VRP Pattern (Incorrect Message Routing/Handling):** More privileged process incorrectly handling a message type intended for a different process type or state.
        *   Browser process handling `ACCEPT_BROKER_CLIENT` (meant for renderer bootstrapping), leading to crash/DCHECK failure (VRP2.txt#370).
    *   **VRP Pattern (Privileged Actions from Untrusted Contexts):** Mojo interfaces callable by renderers allowing simulation of privileged user actions or direct control over system resources.
        *   `StartDragging`: Renderer controlling mouse globally via Mojo IPC, leading to sandbox escape (VRP2.txt#4). See [drag_and_drop.md](drag_and_drop.md).
    *   **VRP Pattern (Input Validation - General):** Interfaces not robustly validating parameters (e.g., sizes for shared buffers `SharedBufferDispatcher`, potentially leading to OOB access if not checked by receiver).

*   **State Management / Race Conditions / Lifetime Issues (Medium Likelihood, High VRP Impact):** Errors related to object lifetimes, concurrent operations, or inconsistent state between processes.
    *   **VRP Pattern (Use-After-Free):** Incorrect lifetime management of objects involved in Mojo communication, especially those tied to UI elements or requests that can be cancelled asynchronously.
        *   `PermissionRequestManager` UAF: Request object deleted but pointer remained in `duplicate_requests_`, potentially leading to incorrect grant later (VRP: `1424437`). See [permissions.md](permissions.md).
        *   Side Panel UAF via `observer_list.h`: Likely related to Mojo observer pattern lifetime issues (VRP: `40061678`). See [side_panel.md](side_panel.md).
    *   **VRP Pattern (Double Fetch / TOCTOU):** Reading data (e.g., for checksum) and then using it later without re-validation, allowing renderer to modify underlying shared buffer in between.
        *   `GeneratedCodeCache::WriteEntry`: Checksum calculated then buffer written later via separate Mojo call (`EnqueueOperation`); potential for mismatch if underlying shared buffer changed (VRP2.txt#542).

*   **MojoJS Interactions (Medium Likelihood, Medium VRP Impact):** Issues specific to Mojo bindings exposed to WebUI (`MojoJS`), often involving lifetime or context separation failures.
    *   **VRP Pattern (Bindings Leakage):** MojoJS bindings enabled for a WebUI page persisting and being accessible to a subsequently navigated (potentially untrusted) site loaded in the same process. (VRP: `40053875`).

*   **Resource Management:** Vulnerabilities related to resource allocation or exhaustion (less common for direct EoP in VRPs, more likely DoS).
    *   **VRP Pattern (DoS/Resource Exhaustion):** Potential via message flooding or exploiting interfaces that allocate significant resources (e.g., large shared buffers via `SharedBufferDispatcher`). WebRTC over Mojo could contribute (VRP2.txt#6058).

## 3. Further Analysis and Potential Issues
*   **Trust Boundaries & Validation:** **The most critical area is validating messages received *across* trust boundaries, especially from Renderer/GPU/Utility processes to the Browser process.** Assume messages from less privileged processes are potentially malicious and require full validation (origin checks, capability checks, parameter sanitization, feature flag checks) in the receiving process *before* acting on them. Do not rely on the renderer to perform security checks.
*   **Origin Tracking & Enforcement:** How is the origin associated with a Mojo request reliably determined (e.g., via `RenderFrameHost::GetLastCommittedOrigin` on the associated frame) and passed to browser-side handlers? Is it consistently checked, especially for interfaces dealing with user data or permissions? The VRPs show multiple instances where origin checks were missing or insufficient.
*   **Interface Exposure:** Audit which Mojo interfaces are exposed to which process types (especially renderers). Minimize exposure of sensitive interfaces. Use mechanisms like `AssociatedInterface` where appropriate to tie interface lifetime to frames.
*   **Generated Code vs. Manual Implementation:** Remember that Mojom definitions and generated bindings handle *syntactic* validation (correct types, structure). **All *semantic* validation and security checks (permissions, origin checks, state validation, capability checks) MUST be implemented manually in the C++ interface implementation.**
*   **State Synchronization & Lifetime:** Analyze complex interactions involving asynchronous operations and shared state managed via Mojo. Look for potential races or inconsistencies (e.g., `PermissionRequestManager` UAF - VRP: `1424437`). Ensure robust lifetime management for objects involved in Mojo calls, especially observers. Use `mojo::Receiver`, `mojo::Remote`, `mojo::ReceiverSet` correctly.
*   `SharedBufferDispatcher`: Investigate potential for memory corruption if size parameters from untrusted processes are not strictly validated before allocation/use by the receiver.

## 4. Code Analysis
*   `NodeController`: Manages Mojo nodes and message routing. Analyze state machine logic, message dispatch (`OnChannelMessage`), handling of bootstrap messages (`OnAcceptBrokerClient` - VRP2.txt#370).
*   `Channel`: Low-level message transport.
*   Dispatchers (e.g., `SharedBufferDispatcher`): Handle specific resource types over Mojo. Analyze resource allocation, validation, and lifetime management.
*   Interface Implementations (`*impl.cc`): **This is the primary area for finding logic bugs.** Analyze implementations of interfaces exposed across trust boundaries, focusing on:
    *   **Validation of all input parameters from the message.**
    *   **Verification of caller permissions/origin *before* performing actions.** (Missing/insufficient checks in VRP: `1275626`, `1263530`, `1263528`; VRP2.txt#14587).
    *   **Re-validation of necessary preconditions** (e.g., feature flags) in the browser process. (Missing in VRP2.txt#225).
    *   **Correct state management** and handling of asynchronous replies.
    *   **Secure interaction** with underlying browser components (e.g., databases, permission stores).
*   Mojom Files (`*.mojom`): Define interfaces and data structures. Useful for understanding intended functionality.
*   Generated Bindings: Handle serialization/deserialization and basic type validation.

## 5. Areas Requiring Further Investigation
*   **Systematic Audit of Renderer-Exposed Interfaces:** Prioritize auditing Mojo interfaces exposed to the renderer process, specifically checking for:
    *   **Missing/Insufficient Origin Validation:** Especially in services related to storage (`ContentIndexService`, `BackgroundFetchService`), permissions (`PushMessaging`), user profiles, or device access. Verify checks use reliable origin sources (e.g., `RenderFrameHost::GetLastCommittedOrigin`) and security policies.
    *   **Lack of Browser-Side Re-validation:** Handlers assuming renderer-provided data (like URLs in `FencedFrame::Navigate` or feature state) is trustworthy.
    *   **Input Sanitization:** Thorough validation of parameters like URLs, file paths, sizes, IDs. Can malformed messages cause issues?
*   **State Management Review:** Focus on components managing state across potentially concurrent Mojo calls (e.g., `PermissionRequestManager`, `GeneratedCodeCache`, `NodeController`). Use thread safety analysis tools. Check for UAFs related to Mojo object lifetimes (Receivers, Remotes, callbacks).
*   **Fuzzing:** Target complex Mojom interfaces exposed to renderers with structured fuzzing.
*   **Site Isolation Interaction:** How does Mojo interact with Site Isolation? Can messages be routed incorrectly or bypass isolation checks, especially during process swaps or navigation?

## 6. Related VRP Reports
*   **Insufficient Validation/Access Control:**
    *   VRP2.txt#4 (`StartDragging` mouse control - Privileged Action)
    *   VRP2.txt#370 (`ACCEPT_BROKER_CLIENT` handling - Incorrect Routing/Handling)
    *   VRP: `1275626` (`PushMessaging` origin check bypass/insufficiency)
    *   VRP: `1263530`, `1263528` (`ContentIndexService` origin check)
    *   VRP2.txt#14587 (`GetDeveloperIdsTask` origin check)
    *   VRP2.txt#225 (`FencedFrame` browser validation bypass)
    *   VRP2.txt#11815 (Extension message permission check - Renderer spoofing extension)
*   **State Management/Race Conditions:**
    *   VRP2.txt#542 (`GeneratedCodeCache` double fetch/TOCTOU)
    *   VRP: `1424437` (`PermissionRequestManager` UAF - Lifetime issue)
    *   VRP: `40061678` (Side panel UAF - potentially involves Mojo observers)
*   **MojoJS Interactions:**
    *   VRP: `40053875` (Bindings leakage after navigation)
*   **(Indirectly Related - EoP via COM):** While not direct Mojo bugs, these show patterns of IPC vulnerabilities in related components.
    *   VRP: `340090047`, VRP2.txt#3763 (COM Session Moniker)
    *   VRP2.txt#1152 (COM `IAppBundleWeb` impersonation)

## 7. Cross-References
*   [ipc.md](ipc.md) (Legacy IPC)
*   [permissions.md](permissions.md)
*   [push_messaging.md](push_messaging.md)
*   [content_index.md](content_index.md)
*   [background_fetch.md](background_fetch.md)
*   [fenced_frames.md](fenced_frames.md)
*   [drag_and_drop.md](drag_and_drop.md)
*   [site_isolation.md](site_isolation.md)

## Additional Notes
Mojo is fundamental to Chromium's multi-process architecture. Ensuring the security of its interfaces, especially those crossing privilege boundaries (Renderer -> Browser being the most critical), is paramount. The pattern of missing or insufficient origin/permission checks in browser-side handlers for renderer-initiated requests appears common based on VRPs and requires careful auditing. Assumptions about renderer-provided data are dangerous.
