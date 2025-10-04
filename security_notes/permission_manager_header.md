# `PermissionManager`: The Central Dispatcher

The `permissions::PermissionManager` class is Chrome's concrete implementation of the `PermissionControllerDelegate` interface. It acts as the central nervous system for permission requests, receiving them from the `PermissionController` and dispatching them to specialized handlers. Its header file, `components/permissions/permission_manager.h`, reveals a sophisticated, context-based architecture.

## Key Architectural Features

### 1. The `PermissionContext` Model

The most significant architectural pattern in `PermissionManager` is its use of a **map of permission contexts**.

-   **`PermissionContextMap`**: `PermissionManager` holds a map (`permission_contexts_`) that associates each `ContentSettingsType` (e.g., `GEOLOCATION`, `NOTIFICATIONS`) with a dedicated `PermissionContextBase` object.
-   **Specialized Handlers**: Each `PermissionContextBase` subclass is an expert on a single permission type. It knows the specific logic required to request that permission, what UI considerations are needed, and how to interpret the results. For example, the `GeolocationPermissionContext` handles geolocation requests, while the `NotificationPermissionContext` handles notifications.
-   **Dispatch Mechanism**: When `PermissionManager` receives a request (e.g., via `RequestPermissions`), it looks up the appropriate `PermissionContextBase` in its map based on the permission type and forwards the request to that context object. This is a clean and extensible design that avoids a monolithic `if/else` or `switch` statement for handling different permissions.

### 2. Asynchronous Request Management

Handling a permission request often involves asynchronous operations, especially waiting for user input from a UI prompt. `PermissionManager` manages this complexity using a system of pending requests.

-   **`PendingRequest`**: When a multi-permission request comes in, it's wrapped in a `PendingRequest` object.
-   **`PendingRequestsMap`**: This map stores all the `PendingRequest` objects, each indexed by a unique ID.
-   **Callback Orchestration**: The `PendingRequest` object holds the final callback that needs to be run once *all* the requested permissions have been resolved. As each individual permission context completes its work (e.g., the user clicks "Allow" or "Block"), it notifies the `PermissionManager`, which updates the corresponding `PendingRequest`. Once all permissions in the request have a result, the `PendingRequest` executes the final callback, sending the collected results back to the `PermissionController`.

### 3. Key Dependencies and Roles

`PermissionManager` inherits from several classes, each defining a key part of its role:

-   **`content::PermissionControllerDelegate`**: This is its primary role. It fulfills the contract required by the `content/` layer to handle permission UI and embedder-specific logic.
-   **`KeyedService`**: This signifies that `PermissionManager` is a per-profile service. Each user profile gets its own instance, ensuring that permissions are not shared between profiles.
-   **`PermissionDecisionAutoBlocker::Observer`**: `PermissionManager` observes the `PermissionDecisionAutoBlocker`. This is a crucial security feature that combats permission spam. If a site repeatedly requests a permission that the user dismisses, the auto-blocker can place that permission under an "embargo," causing future requests to be automatically denied without a prompt. `PermissionManager`'s role as an observer allows it to be aware of these embargoes when making decisions.

## Security Implications

The architecture revealed in the header file has several positive security implications:

-   **Separation of Concerns**: By delegating permission-specific logic to dedicated `PermissionContext` objects, the code is made more modular and easier to audit. An expert on notifications can focus on `NotificationPermissionContext` without needing to understand the intricacies of every other permission.
-   **Centralized Control Flow**: While logic is delegated, `PermissionManager` remains the central point of entry. This ensures that common pre-processing and post-processing of requests can be handled consistently.
-   **Defense Against Spam**: The integration with the `PermissionDecisionAutoBlocker` shows a proactive defense against abusive request patterns, protecting the user from being harassed by prompts.

Analyzing this header makes it clear that the next step is to understand how the `PermissionManager` implements the delegate methods and dispatches to the various `PermissionContext` objects.