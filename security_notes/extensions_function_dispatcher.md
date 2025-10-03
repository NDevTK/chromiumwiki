# Security Notes: `extensions/browser/extension_function_dispatcher.cc`

## File Overview

This file implements the `ExtensionFunctionDispatcher`, which is the central gatekeeper for all extension API calls originating from renderer processes (including extension frames, content scripts, and service workers). When a script in a renderer calls an API like `chrome.tabs.create()`, the request is routed to this dispatcher in the browser process. This class is responsible for validating the request, creating the appropriate C++ handler (`ExtensionFunction`), and executing it with the correct privileges and context. It is one of the most critical security boundaries in the extension system.

## Key Security Mechanisms and Patterns

### 1. Rigorous Request Validation (`ValidateRequest`)

Before any other logic, the dispatcher performs a series of critical validation steps. A failure at this stage is considered a severe security violation, and the standard response is to terminate the offending renderer process via `bad_message::ReceivedBadMessage`.

-   **Context-Type Verification**: The dispatcher ensures the request's context is consistent. A request cannot claim to be from both a `RenderFrameHost` and a service worker simultaneously. This prevents basic context confusion attacks.
-   **Process-Origin Lock Enforcement**: The most important check is `util::CanRendererActOnBehalfOfExtension`. This function verifies that the renderer process making the request is authorized to host the extension it claims to be. This is the primary defense against a compromised renderer (e.g., one hosting a malicious web page) from impersonating a privileged extension to call its APIs.

### 2. Privileged Context and Origin Checks

After initial validation, the dispatcher performs more granular checks on the context of the request.

-   **`CanProcessHostContextType`**: This check, performed against the `ProcessMap`, ensures that the specific extension is allowed to run in the context it claims. For example, it prevents a regular web page context (`kWebPage`) from executing an API that is restricted to a privileged extension background context (`kPrivilegedExtension`).
-   **WebUI Isolation**: The dispatcher has specific logic for `kUntrustedWebUi` contexts, ensuring that these requests originate from `chrome-untrusted://` URLs and are not associated with an extension. This enforces the security boundary between WebUI and the extension system.

### 3. Permission Enforcement Flow

The dispatcher orchestrates the standard permission check for every API call.

1.  It uses the `ExtensionFunctionRegistry` to look up the requested API name (e.g., "tabs.create") and create an instance of the corresponding `ExtensionFunction` subclass (e.g., `TabsCreateFunction`).
2.  Crucially, immediately after creating the function object, it calls `function->HasPermission()`.
3.  This method on the base `ExtensionFunction` class checks if the extension's manifest (`permissions` key) grants access to the specific API being called.
4.  If `HasPermission()` returns `false`, the dispatcher immediately aborts the request and responds with a generic "Access to extension API denied" error. The function's `Run()` method is never called.

This ensures a consistent, non-bypassable permission check at the entry point of every API call.

### 4. Denial-of-Service Prevention (Quota Management)

To prevent a single extension from overwhelming the browser with API calls, the dispatcher integrates with the `QuotaService`.

-   **`quota->Assess(...)`**: Before executing the function, the dispatcher calls this method. The `QuotaService` checks the extension's call rate against pre-defined buckets (e.g., calls per minute, calls per hour) for that specific API.
-   **Throttling**: If the extension has exceeded its quota, `Assess()` returns a violation error, and the function is not executed. Instead, the `OnQuotaExceeded()` method is called, which returns an error to the extension. This is a critical defense against denial-of-service attacks.

### 5. Lifecycle Management and Keepalives

Event-driven extensions (those with background service workers or event pages) are not always running. The dispatcher is responsible for ensuring the extension's background context remains alive for the duration of an API call.

-   **`ServiceWorkerKeepalive` / `IncrementLazyKeepaliveCount`**: Before a function's `Run()` method is executed, the dispatcher acquires a "keepalive" for the extension's service worker or event page.
-   **`OnExtensionFunctionCompleted`**: When the `ExtensionFunction` completes (either synchronously or asynchronously), it notifies the dispatcher via this method, which then releases the keepalive.
-   **Security Implication**: The correctness of this keepalive logic is critical for both stability and security. A bug could lead to a resource leak (if keepalives are not released) or a use-after-free vulnerability if the extension's background process is terminated prematurely while an API call is still in flight.

## Summary of Security Posture

The `ExtensionFunctionDispatcher` is a robust, multi-layered security gatekeeper.

-   **Security Model**: It operates on a "deny by default" principle, where a request must pass a series of increasingly specific checks (process validity, context type, permissions, quotas) before it is allowed to execute.
-   **Fail-Fast and Fail-Loud**: It uses `bad_message` to terminate renderers that send fundamentally invalid requests, providing a strong deterrent and clear signal of security violations.
-   **Centralized Control**: By centralizing the dispatch logic, it ensures that all extension APIs are subject to the same core set of security checks, reducing the risk of an individual API implementation forgetting a critical validation step.