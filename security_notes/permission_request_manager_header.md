# `PermissionRequestManager`: The UI Gatekeeper

The `PermissionRequestManager`, whose interface is defined in `components/permissions/permission_request_manager.h`, is the component responsible for managing the UI that prompts the user for permission. It acts as the final gatekeeper, deciding if, when, and how a permission prompt is shown. It is a per-tab class, with one instance for each `WebContents`.

## Core Responsibilities and Architecture

The header file reveals several key architectural decisions that are central to its function and security.

### 1. Request Queuing and Coalescing

-   **`PermissionRequestQueue`**: The manager doesn't necessarily show a prompt for every request as it arrives. It maintains a `pending_permission_requests_` queue. This is a crucial feature to prevent prompt spam and to group related requests.
-   **Request Merging**: The manager can merge identical incoming requests with one that is already pending or being shown, rather than creating a duplicate. This provides a cleaner user experience.
-   **Scheduling**: Requests are not always dequeued immediately. The manager uses a timer (`ScheduleDequeueRequestIfNeeded`) to wait briefly, allowing multiple requests triggered in quick succession (e.g., by the same script) to be coalesced into a single UI prompt.

### 2. The `PermissionPrompt` and `PermissionPrompt::Delegate`

-   **Abstraction of the View**: The `PermissionRequestManager` does not draw any UI itself. It uses a `PermissionPrompt` object, which is an abstract representation of the prompt view. The actual view can be a bubble, an infobar, or a native Android dialog.
-   **Delegate Role**: The manager implements the `PermissionPrompt::Delegate` interface. This means it is the "controller" for the prompt "view". When the user clicks "Allow", "Block", or "Dismiss" on the UI, the `PermissionPrompt` calls the corresponding method (`Accept()`, `Deny()`, `Dismiss()`) on the `PermissionRequestManager`. The manager is then responsible for informing the `PermissionRequest` object of the user's decision.

### 3. Deciding the UI: `PermissionUiSelector`

-   **Quiet UI vs. Loud UI**: One of the most important modern features of the permission system is the ability to show a less intrusive "quiet" prompt (e.g., a small chip in the address bar) for sites that are suspected of abusing permission requests.
-   **A Chain of Selectors**: The manager holds a vector of `PermissionUiSelector` objects. Each selector is a rule engine that can vote on whether to show the normal (loud) prompt or the quiet UI. This chain of selectors allows for multiple heuristics to be applied (e.g., prediction service, abusive request blacklists, user settings). The decision of the highest-priority selector wins.
-   **Security Implication**: This is a direct defense against notification spam and other abusive permission patterns. By degrading the UI for untrusted sites, the browser reduces the likelihood of users granting permissions they don't intend to.

### 4. Lifecycle and State Management

-   **Per-Tab Instance**: As `WebContentsUserData`, the manager's lifecycle is tied to a single tab.
-   **`WebContentsObserver`**: It observes the `WebContents` for events like navigations and tab visibility changes.
    -   **Navigation**: If the main frame navigates, `CleanUpRequests()` is called to dismiss any visible prompt and clear all pending requests. This is a critical security measure to ensure a permission prompt from a previous page doesn't linger over a new page.
    -   **Visibility**: It tracks tab visibility (`OnVisibilityChanged`). Prompts are generally not shown for background tabs to prevent non-visible pages from spamming the user.

## Security Posture

The `PermissionRequestManager`'s design, as seen in its header, is heavily focused on creating a secure and user-friendly experience by managing the "prompt budget."

-   **Prompt Spam Prevention**: The queuing and coalescing logic is the primary defense against a page overwhelming the user with multiple prompts.
-   **UI Redressing Defense**: The logic for deciding between quiet and loud UI is a direct countermeasure against sites that engage in abusive or deceptive request patterns.
-- **Context-Awareness**: Tying the prompt lifecycle to tab navigation and visibility ensures that prompts are only shown in the correct context, preventing a prompt from one site from being used to grant a permission to another.

This class is the nexus of permission policy, UI, and user interaction. Its implementation will reveal the fine-grained details of how these competing concerns are balanced.