# PermissionController (`content/browser/permissions/permission_controller_impl.h`)

## 1. Summary

The `PermissionControllerImpl` is a singleton per `BrowserContext` and acts as the central authority in the browser process for managing permissions for powerful web platform features. It is the ultimate decision-maker for granting, denying, and querying the status of permissions such as geolocation, notifications, camera/microphone access, and more.

This class sits at a critical security boundary. It receives permission requests from untrusted renderer processes and is responsible for safely exposing the UI prompts to the user and ensuring that the principle of least privilege is upheld. A logical flaw in this controller could lead to a website gaining sensitive capabilities without user consent, or to information leaks about the user's settings.

## 2. Core Concepts

*   **Centralized Decision Making:** All permission-related questions from any frame or worker within a `BrowserContext` are routed to its single `PermissionControllerImpl` instance. This ensures that a consistent set of policies is applied.

*   **Query vs. Request:** The controller handles the two main functions of the Permissions API:
    1.  **Query (`GetPermissionStatusFor...`):** A low-friction way for a website to check the current status of a permission (`granted`, `denied`, or `prompt`). This does not trigger a user prompt.
    2.  **Request (`RequestPermissionsFromCurrentDocument`):** An active request for a permission, which is expected to trigger a user-facing prompt to obtain consent.

*   **Origin-Based Permissions:** All permission decisions are keyed primarily by the **requesting origin** (the origin of the frame asking for the permission) and the **embedding origin** (the origin of the top-level page). This distinction is critical for security, as it allows the browser to apply different policies for a third-party iframe versus a first-party context.

*   **DevTools/Testing Overrides:** The class exposes a powerful set of `...ForDevTools` methods (`SetOverrideForDevTools`, `GrantOverridesForDevTools`). These methods allow browser tests and the DevTools inspector to programmatically grant or deny permissions for specific origins, bypassing the normal user-prompt flow entirely.

## 3. Security-Critical Logic & Vulnerabilities

*   **Origin Validation and Spoofing:**
    *   **Risk:** The most critical security responsibility of this class is to correctly identify the origin that is requesting a permission. If a malicious script from `evil.com` could make a request that the `PermissionControllerImpl` believes is coming from `bank.com`, it could be granted access to sensitive permissions based on `bank.com`'s trusted status.
    *   **Mitigation:** The controller receives the `RenderFrameHost` or `RenderProcessHost` along with the request, allowing it to securely query the true, browser-verified origin of the requester. It must never trust an origin string sent directly from the renderer process.

*   **DevTools Override Security:**
    *   **Risk:** The override functionality is a "keys to the kingdom" feature. If a compromised renderer could find any way to invoke `SetOverrideForDevTools`, it could grant itself any permission it desires without user interaction.
    *   **Mitigation:** The security of this feature relies entirely on the Mojo IPC layer. These methods should only be exposed on an interface that is exclusively available to trusted browser processes (like the DevTools backend) and never to a renderer. A flaw in the `BrowserInterfaceBroker` policy map that incorrectly exposed this interface would be a critical vulnerability.

*   **Prompt Integrity and Spoofing:**
    *   **Risk:** While this class doesn't draw the UI itself, it is responsible for initiating the request that leads to a UI prompt. It provides the origins and permission details to be displayed. If incorrect information were passed to the UI layer, a prompt could be shown that misleads the user about which site is requesting the permission.
    *   **Implementation:** The `RequestPermissions` method takes a `PermissionRequestDescription` struct which contains the origins and the list of permissions to be displayed to the user. The integrity of this data is critical.

*   **Information Leaks:**
    *   **Risk:** The `query()` API can be used by malicious sites as a fingerprinting vector. While this is allowed by the specification, a bug in the controller that leaks more information than necessary—for example, the precise reason a permission is blocked, or the status of permissions for unrelated origins—would exacerbate this risk.
    *   **Subscription Leaks:** The `SubscribeToPermissionResultChange` mechanism could leak information if it incorrectly notifies a listener in one origin about a permission change that occurred for a different origin.

## 4. Key Functions

*   `GetPermissionResultForCurrentDocument(...)`: The primary entry point for `permission.query()`.
*   `RequestPermissionsFromCurrentDocument(...)`: The primary entry point for `permission.request()`.
*   `SetOverrideForDevTools(...)`: A highly privileged method for bypassing the standard permission flow for testing and debugging.
*   `SubscribeToPermissionResultChange(...)`: Allows a renderer to listen for changes to a permission's status.
*   `ResetPermission(...)`: Allows the user (or other browser systems) to revoke a permission for an origin.

## 5. Related Files

*   `content/public/browser/permission_controller_delegate.h`: An interface that allows the `content` layer to be embedded in different products (e.g., Chrome, WebView), each with its own specific permission logic and UI. The `PermissionControllerImpl` uses this delegate to get the final permission status.
*   `content/browser/permissions/permission_service_impl.h`: The Mojo service implementation that runs in the browser and handles IPCs from the renderer, forwarding them to the `PermissionControllerImpl`.
*   The various UI files that implement the actual permission bubbles/prompts (e.g., in `chrome/browser/ui/views/permissions/`).