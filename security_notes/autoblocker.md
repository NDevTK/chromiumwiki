# The `PermissionDecisionAutoBlocker`: Defending Against Spam

The `PermissionDecisionAutoBlocker` is a profile-wide service that acts as a crucial defense against permission request spam. Its primary purpose is to identify origins that exhibit abusive request patterns (e.g., repeatedly asking for a permission the user ignores or dismisses) and place them under a temporary "embargo," automatically blocking future requests from that origin without prompting the user. The public interface for this class is defined in `components/permissions/permission_decision_auto_blocker.h`.

## Core Functionality and API

The header file reveals the key public methods that form the autoblocker's API.

### Recording User Actions

The `PermissionRequestManager` is responsible for notifying the autoblocker of user actions on a prompt. The autoblocker provides specific methods for this:

-   **`RecordDismissAndEmbargo(...)`**: This is called when a user explicitly dismisses a permission prompt (e.g., by clicking the 'X' button).
-   **`RecordIgnoreAndEmbargo(...)`**: This is called when a user ignores a prompt (e.g., by clicking away, switching tabs, or if a quiet chip prompt times out).

Both methods take a `bool` argument indicating if the prompt was a "quiet" UI. This is significant because the thresholds for triggering an embargo are different for quiet and loud prompts. A user ignoring a loud, intrusive prompt is a stronger signal of annoyance than ignoring a subtle, quiet one.

### Checking for Embargo

-   **`IsEmbargoed(origin, permission)`**: This is the primary query method. Other parts of the system, like a `PermissionContext`, can call this to check if a request from a given origin for a specific permission should be automatically blocked *before* attempting to show a prompt.

### Managing Embargo

-   **`RemoveEmbargoAndResetCounts(origin, permission)`**: This method provides an escape hatch. It allows the system to lift an embargo and reset the dismiss/ignore counts for an origin. This is typically used when a user takes a clear, positive action, such as manually granting the permission through the site settings page. This signals that the user's intent has changed, and the site should be given another chance.

## How It Works: Storing State in `HostContentSettingsMap`

The `PermissionDecisionAutoBlocker` does not have its own persistent storage. It leverages the existing `HostContentSettingsMap` to store its data. The header file hints at this with several private `const char*` keys:

-   `kPromptDismissCountKey`
-   `kPromptIgnoreCountKey`
-   `kPromptDismissCountWithQuietUiKey`
-   `kPromptIgnoreCountWithQuietUiKey`
-   `kPermissionDismissalEmbargoKey`
-   `kPermissionIgnoreEmbargoKey`

When `RecordDismissAndEmbargo` is called, the autoblocker retrieves the current dismiss count from a dictionary stored in the website settings for that origin, increments it, and writes it back. If the new count exceeds a predefined threshold, it then sets another key to place the origin under embargo.

## Security and Usability Implications

-   **Rate-Limiting Annoyance**: The autoblocker is essentially a rate-limiter for user annoyance. It prevents a malicious or poorly designed site from trapping a user in a loop of permission prompts.
-   **Clear Path to Redemption**: The `RemoveEmbargoAndResetCounts` function is critical. It ensures that the embargo is not a permanent "death sentence." If a user changes their mind and decides to grant a permission, the system can gracefully recover.
-   **Differentiated UI Logic**: The distinction between quiet and loud prompts is important. It allows the browser to be more lenient with quiet UI, which is less intrusive, while being stricter with loud prompts that interrupt the user's workflow.
-   **Centralized State**: By storing its state in the `HostContentSettingsMap`, the embargo data is tied to the user's profile and is cleared along with other site data when the user clears their browsing history.

The `PermissionDecisionAutoBlocker` is a key component in Chromium's strategy to make permissions powerful for developers without being overwhelming or dangerous for users. The next step is to analyze its implementation to understand the specific thresholds and logic used to trigger an embargo.

## Implementation Details: Thresholds and Embargo Logic

The implementation in `permission_decision_auto_blocker.cc` reveals the specific rules that trigger an embargo. The logic is based on simple counters that are incremented on each user action.

### Embargo Thresholds

The system uses different thresholds for normal ("loud") prompts versus the less intrusive "quiet" UI prompts. The default thresholds are defined as constants:

-   **Loud Prompts**:
    -   Dismissals: **3** (`kDefaultDismissalsBeforeBlock`)
    -   Ignores: **4** (`kDefaultIgnoresBeforeBlock`)
-   **Quiet Prompts**:
    -   Dismissals: **1** (`kDefaultDismissalsBeforeBlockWithQuietUi`)
    -   Ignores: **2** (`kDefaultIgnoresBeforeBlockWithQuietUi`)

When a user dismisses or ignores a prompt, the `RecordDismissAndEmbargo` or `RecordIgnoreAndEmbargo` method is called. This function increments the relevant counters stored in the `HostContentSettingsMap`. If the new count reaches the threshold, the `PlaceUnderEmbargo` function is called.

### Embargo Duration

-   **Standard Embargo**: For most permissions, a standard embargo lasts for **7 days** (`kDefaultEmbargoDays`).
-   **Specialized Embargo**: Some newer or more sensitive permissions have custom embargo logic. For example, the `FEDERATED_IDENTITY_API` has a much more aggressive and progressive embargo:
    -   The first dismissal triggers a **2-hour** embargo.
    -   Subsequent dismissals trigger embargoes of **1 day**, **7 days**, and finally **28 days**.

### The Embargo Mechanism

-   **Placing an Embargo**: The `PlaceUnderEmbargo` function doesn't set a simple boolean flag. Instead, it writes the **current timestamp** into the website's settings under a specific key (e.g., `kPermissionDismissalEmbargoKey`).
-   **Checking for Embargo**: When `IsEmbargoed` is called, it retrieves this stored timestamp. It then checks if `now() < stored_timestamp + embargo_duration`. If the current time is still within the embargo window, the check returns `true`, and the permission request is automatically blocked.

This timestamp-based mechanism is simple, effective, and ensures that embargoes automatically expire after the configured duration without requiring a separate cleanup process.