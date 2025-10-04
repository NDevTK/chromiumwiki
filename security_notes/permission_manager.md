# `PermissionManager`: The Implementation of the Central Dispatcher

The `permissions::PermissionManager` class, implemented in `components/permissions/permission_manager.cc`, is the concrete realization of the `PermissionControllerDelegate` interface in Chrome. It serves as the central dispatcher for all permission-related queries and requests, orchestrating the flow between the `PermissionController`, various permission-specific `PermissionContext` objects, and the UI-handling `PermissionRequestManager`.

## Core Logic: The Dispatch Mechanism

The `PermissionManager`'s primary role is not to make permission decisions itself, but to route requests to the object that can. This is clearly visible in its core methods.

### `RequestPermissionsInternal`

This method is the entry point for all permission requests that might require a user prompt. Its logic is a clear example of the dispatcher pattern:

1.  **Create a `PendingRequest`**: The entire request, which might contain multiple permissions, is encapsulated in a `PendingRequest` object. This object tracks the state of all the individual permissions and holds the final callback to be run when all are resolved.
2.  **Iterate and Dispatch**: The method iterates through each permission in the request.
3.  **Find the Right Handler**: For each permission, it calls `GetPermissionContext(permission_type)` to retrieve the appropriate `PermissionContextBase` subclass from its map (e.g., `GeolocationPermissionContext`, `NotificationPermissionContext`).
4.  **Check for Kill Switches**: It performs initial checks, such as whether the permission is blocked by a feature policy (`PermissionUtil::IsPermissionBlockedInPartition`) or if the entire permission is disabled via a kill switch (`context->IsPermissionKillSwitchOn()`). If so, it denies the request immediately without consulting the context.
5.  **Delegate to the Context**: If the initial checks pass, it calls `context->RequestPermission(...)`. This is the crucial hand-off. The `PermissionManager`'s job is done for now, and the specialized `PermissionContext` takes over to handle the permission-specific logic.
6.  **Provide a Callback**: It passes a `PermissionResponseCallback` to the context. This callback, when invoked by the context, will notify the `PermissionManager` of the result for that single permission.

### `GetPermissionStatusInternal`

This method handles status queries (like `navigator.permissions.query()`). The logic is similar but simpler:

1.  **Find the Handler**: It calls `GetPermissionContext()` to get the correct context for the permission type.
2.  **Delegate the Query**: It calls `context->GetPermissionStatus()`. The context is responsible for checking the `HostContentSettingsMap` and any other relevant state.
3.  **Incorporate Device Status**: It honors the `should_include_device_status` flag by calling `context->UpdatePermissionStatusWithDeviceStatus()`, ensuring the final result reflects the reality of the underlying OS.

## Asynchronous Flow and UI Management

The `PermissionManager` does not directly manage UI. This is a critical separation of concerns.

-   The `PermissionContext` objects are responsible for deciding *if* a prompt is needed.
-   If a prompt is required, the context will typically call `PermissionRequestManager::FromWebContents(web_contents)->AddRequest(...)`.
-   The `PermissionRequestManager` is a per-tab object responsible for queuing permission requests and displaying the UI (e.g., the permission bubble). It ensures that multiple requests from the same tab are handled gracefully, often grouping them into a single prompt.
-   When the user interacts with the prompt, the `PermissionRequestManager` calls back to the `PermissionContext`, which in turn calls the `PermissionResponseCallback` to notify the `PermissionManager`.

This flow ensures that `PermissionManager` remains a backend dispatcher, decoupled from the complexities of UI management.

## Security Analysis

-   **Robust and Extensible Design**: The context-based architecture is highly robust. Adding a new permission type to Chromium is a matter of creating a new `PermissionContext` subclass and registering it with the `PermissionManager`. This avoids modifying a central, monolithic class and reduces the risk of unintended side effects.
-   **Centralized Pre-Checks**: By handling requests centrally, `PermissionManager` can enforce universal security checks before dispatching. For example, the check for feature policy blocks is done here, ensuring it's applied consistently for all permission types.
-   **Clear Ownership and Lifecycles**: The use of `PendingRequest` and `PermissionResponseCallback` ensures that the state of asynchronous requests is managed cleanly. Callbacks are guaranteed to be handled, and there's a clear path for tearing down a request if the renderer frame disappears, preventing leaks or zombie requests.
-   **Decoupling from UI**: Separating the dispatch logic (`PermissionManager`) from the UI logic (`PermissionRequestManager`) is a strong security design. It allows UI experts to focus on building secure, non-spoofable prompts without needing to understand the intricacies of the permission backend, and vice-versa.