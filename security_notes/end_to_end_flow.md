# End-to-End Flow of a Chromium Permission Request

This document provides a high-level summary of the entire lifecycle of a permission request in Chromium, tracing it from the initial JavaScript call in a web page to the final storage of the user's decision. It connects the various components analyzed in the detailed security notes.

---

### 1. Initiation (Renderer Process)

A permission request begins when a web page attempts to use a feature that requires special user consent.

-   **JavaScript Call**: A script calls an API like `navigator.geolocation.getCurrentPosition()` or `Notification.requestPermission()`.
-   **Blink Implementation**: This call is handled by a corresponding class in Blink, such as `Geolocation`. This class is responsible for initiating an asynchronous request to the browser process.
-   **Mojo IPC**: The request is packaged and sent over a Mojo IPC (Inter-Process Communication) pipe to the browser process's `PermissionController`.

> For a detailed look at the renderer-side process for a specific API, see:
> `security_notes/geolocation_renderer_flow.md`

---

### 2. Initial Triage (`PermissionController`)

The `PermissionControllerImpl` is the first stop in the browser process. It acts as a gatekeeper, performing initial high-level security checks before any complex logic is invoked.

-   **Security Checks**: It immediately validates the request against several security boundaries:
    -   **Fenced Frames**: Requests from fenced frames are denied.
    -   **Permissions Policy**: If the feature is disabled by the site's Permissions Policy, the request is denied.
    -   **Back-Forward Cache**: Requests from inactive pages are denied.
-   **Delegation**: If these checks pass, the `PermissionController` delegates the request to its `PermissionControllerDelegate`.

> For more details on this initial stage, see:
> `security_notes/permission_request_flow.md`

---

### 3. Dispatch (`PermissionManager`)

In Chrome, the `PermissionControllerDelegate` is implemented by the `PermissionManager`. This class acts as a central dispatcher.

-   **Context Lookup**: The `PermissionManager` receives the request and determines its type (e.g., `GEOLOCATION`, `NOTIFICATIONS`). It then looks up the appropriate, specialized `PermissionContextBase` handler from its internal map.
-   **Hand-off**: The request is forwarded to the specific `PermissionContext` for handling. The `PermissionManager`'s job is now to wait for the context to report back with a result.

> For more details on the dispatcher architecture, see:
> `security_notes/permission_manager.md` and `security_notes/permission_manager_header.md`

---

### 4. UI Management (`PermissionRequestManager`)

If the `PermissionContext` determines that user consent is required (i.e., the current setting is `ASK`), it forwards the request to the `PermissionRequestManager`. This class owns the entire UI lifecycle.

-   **Queuing and Coalescing**: The request is added to a queue. The manager may wait briefly to see if other related requests arrive, which can be grouped into a single prompt.
-   **UI Selection**: It uses a chain of `PermissionUiSelector` objects to decide whether to show a standard, prominent prompt or a less intrusive "quiet" UI. This decision is based on factors like the site's reputation and the user's past interactions.
-   **Prompt Display**: It creates and displays the actual `PermissionPrompt` UI to the user.
-   **Interaction Handling**: It listens for the user's decision (Allow, Deny, Dismiss) and for external events like tab switching or navigation, which can cause the prompt to be hidden or dismissed.

> For a deep dive into the UI layer, see:
> `security_notes/permission_request_manager_header.md` and `security_notes/permission_request_manager.md`

---

### 5. Resolution and Storage (`HostContentSettingsMap`)

Once the user interacts with the prompt, the decision flows back up the chain.

-   The `PermissionRequestManager` informs the `PermissionContext` of the user's choice.
-   The `PermissionContext` is responsible for translating this into a `ContentSetting` (`ALLOW` or `BLOCK`).
-   It then calls the `HostContentSettingsMap` to persist this setting. The map's `PrefProvider` writes the rule (e.g., `(https://example.com, *, GEOLOCATION) -> ALLOW`) to the user's profile on disk.
-   The final result is propagated back through the chain to the original JavaScript callback in the renderer process.

> For more on how settings are stored and retrieved, see:
> `security_notes/permission_storage_and_enforcement.md`

---

This end-to-end flow, with its clear separation of responsibilities, provides a robust and secure framework for managing access to powerful web platform features. Each component acts as a check on the others, creating a layered defense that protects the user while enabling rich web experiences.