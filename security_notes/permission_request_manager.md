# `PermissionRequestManager`: The UI Gatekeeper Implementation

The `PermissionRequestManager`, implemented in `components/permissions/permission_request_manager.cc`, is where the abstract concepts of permission UI management are put into practice. It's a complex class that juggles incoming requests, UI decisions, and user interactions to present a secure and coherent experience.

## The Request Lifecycle in Detail

### 1. Adding a Request (`AddRequest`)

When a `PermissionContext` determines a prompt is needed, it calls `AddRequest`. This method is the primary entry point and performs several critical actions:

-   **Initial Vetting**: It immediately rejects requests from several invalid states:
    -   If the global `kDenyPermissionPrompts` command-line switch is present (for testing).
    -   If the source frame is inactive (e.g., in the Back-Forward Cache) or is a fenced frame. This is a crucial security boundary.
    -   If a "cooldown" is active for notification prompts (i.e., the user recently denied a notification prompt in the same tab).
-   **Auto-Approval**: It checks with `PermissionsClient::Get()->GetAutoApprovalStatus(...)`. This allows for enterprise policies or other embedder-specific logic to automatically grant certain permissions without a prompt.
-   **Deduplication**: It checks if the incoming request is a duplicate of one already pending or being shown. If so, it merges them instead of creating a new prompt.
-   **Queuing**: The request is added to the `pending_permission_requests_` queue.

### 2. Dequeuing and Scheduling (`DequeueRequestIfNeeded`, `ScheduleDequeueRequestIfNeeded`)

-   **Asynchronous Dequeuing**: Requests are not shown immediately. `ScheduleDequeueRequestIfNeeded` posts a task to the UI thread to dequeue the next request. This short delay is critical for **request coalescing**. It allows multiple requests fired in rapid succession from the same script to be gathered and potentially shown in a single prompt.
-   **Grouping**: When `DequeueRequestIfNeeded` runs, it pulls the first valid request from the queue. It then continues to pull subsequent requests as long as they `ShouldGroupRequests` (e.g., microphone and camera requests are often grouped).
-   **Validation**: Before showing a request, it double-checks that its source frame is still active using `HasActiveSourceFrameOrDisallowActivationOtherwise`.

### 3. Deciding the UI (`OnPermissionUiSelectorDone`)

-   After a group of requests is dequeued, the manager consults its `permission_ui_selectors_`.
-   Each selector runs its logic asynchronously and returns a `UiDecision`. This decision dictates whether to use the normal "loud" prompt or a "quiet" one, and for what reason (e.g., `kTriggeredDueToAbusiveRequests`).
-   The manager respects the priority of the selectors, waiting for higher-priority ones to finish before acting on the decision of a lower-priority one.

### 4. Showing the Prompt (`ShowPrompt`)

-   Once the UI decision is made, `ShowPrompt` is called.
-   It uses a factory, `view_factory_`, to create the actual `PermissionPrompt` UI object. This abstracts the platform-specific view creation.
-   It notifies observers that a prompt has been added and logs the appropriate UMA metrics.
-   If the UI selector decided on a quiet prompt for an abusive site, it also calls `LogWarningToConsole` to inform the site developer why they are seeing a degraded UI.

### 5. Handling User Decisions (`Accept`, `Deny`, `Dismiss`, `Ignore`)

-   These methods are called by the `PermissionPrompt` view when the user interacts with it.
-   They iterate through the active requests and inform each `PermissionRequest` object of the user's decision.
-   They record the action for UMA and for the `PermissionDecisionAutoBlocker`. For example, a `Dismiss` or `Ignore` action increments the site's dismiss/ignore count, which can lead to an automatic embargo if the count gets too high.
-   Crucially, they call `FinalizeCurrentRequests` to tear down the UI and kick off the dequeuing of the next pending request.

## Security-Critical Implementation Details

-   **Navigation Cleanup (`DidFinishNavigation`)**: When a top-level navigation commits, the manager calls `CleanUpRequests()`. This immediately cancels all pending and active requests. This is a **vital security measure** that prevents a prompt from a previous, potentially malicious, page from lingering and tricking the user into granting a permission to a new, unrelated page.
-   **Tab Visibility (`OnVisibilityChanged`, `OnTabActiveChanged`)**: The manager carefully tracks whether its tab is active. Prompts are generally not created for hidden tabs. If a tab with an active prompt is hidden, the prompt is typically destroyed and the request is either re-queued or ignored, depending on the prompt type. This prevents background tabs from interrupting the user.
-   **Embargo Integration**: By calling `autoblocker->RecordDismissAndEmbargo` and `autoblocker->RecordIgnoreAndEmbargo`, the manager actively participates in the browser's defense against permission spam. This ensures that abusive sites are automatically quieted down, protecting the user from harassment.
-   **Pre-Ignoring Quiet Prompts (`PreIgnoreQuietPromptInternal`)**: For quiet prompts (like the chip), the manager starts a short timer. If the user doesn't interact with the chip within a few seconds, it's automatically considered "ignored". This prevents a quiet prompt from lingering indefinitely and ensures that the site gets a clear signal, while still protecting the user from a full-sized interruption.