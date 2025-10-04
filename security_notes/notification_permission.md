# The Notification Permission Security Model

The permission to display web notifications is one of the most powerful and frequently abused permissions in the web platform. As a result, Chromium has a highly specialized and restrictive security model for it, implemented primarily in the `NotificationPermissionContext`. The header file `chrome/browser/notifications/notification_permission_context.h` provides a clear overview of these restrictions.

## Core Principles and Restrictions

Unlike more straightforward permissions, the notification permission is subject to a number of strict, upfront checks before a user is ever prompted.

1.  **Secure Origin Requirement**: Notification permission can only be *requested* from a secure origin (i.e., HTTPS). An attempt to request permission from an `http://` page will be immediately rejected. This is a fundamental security measure to prevent network attackers from spoofing notification requests or intercepting push data.

2.  **Top-Level Frame Requirement**: Permission can only be requested from a top-level frame. A request originating from within an `<iframe>` will be automatically rejected. This prevents third-party ads or malicious embedded content from spamming the user with permission prompts that appear to come from the main site. While an iframe can *use* a permission that has already been granted to the top-level origin, it cannot request it.

3.  **No Incognito Mode Support**: Notification permission is **explicitly disabled** in Incognito mode. The header file documentation clearly states this is because of unresolved privacy questions regarding long-lived push subscriptions and the handing of potentially sensitive data to the underlying OS notification center.
    -   **Anti-Fingerprinting**: To prevent a website from detecting that it's running in Incognito mode (which would be a privacy leak), permission requests are not immediately rejected. Instead, they are **automatically rejected after a random delay** of 1-2 seconds. This makes it difficult for a script to distinguish between a user in Incognito and a user who is simply slow to respond to a real prompt.

## Platform-Specific Implementations

The context also highlights the need for platform-specific logic:

-   **Android Notification Channels**: On Android O (SDK 26) and newer, notification permissions are not just a Chrome setting. They are deeply integrated with the Android operating system's concept of "Notification Channels." A user can grant permission in Chrome, but then further configure or disable that permission in the Android system settings. The `NotificationPermissionContext` must interact with the `NotificationChannelsProviderAndroid` to ensure that the browser's permission state is always in sync with the OS.

-   **Extensions**: Extensions have their own permission model. If an extension declares the `"notifications"` permission in its `manifest.json`, it is granted the permission upon installation. The `NotificationPermissionContext` contains special logic (`GetPermissionStatusForExtension`) to check for this manifest permission, bypassing the usual user-prompt flow for these trusted components.

## The `DecidePermission` Method

This is the key method overridden from the base class. The implementation of this method will contain the core logic for:

-   Enforcing the secure origin and top-level frame rules.
-   Handling the automatic rejection in Incognito mode.
-   Kicking off the UI flow by calling the `PermissionRequestManager` when a prompt is required for a regular website.

The security model for notifications is a clear example of a "defense-in-depth" approach. It doesn't rely solely on the user prompt. Instead, it uses a series of strict, technical preconditions to filter out a large number of potentially abusive or insecure requests before the user is ever involved.

## Implementation Details from `DecidePermission`

The `DecidePermission` method in `notification_permission_context.cc` is the concrete implementation of the security model. Here's how it enforces the rules:

1.  **Cross-Origin Check**: The very first check in the method is `if (request_data->requesting_origin != request_data->embedding_origin)`. If the requesting origin does not match the top-level embedding origin, the request is immediately denied. This is the direct implementation of the "top-level frame" requirement.

2.  **Incognito Mode Handling**: The next block explicitly checks `if (browser_context()->IsOffTheRecord())`.
    - It uses a `VisibilityTimerTabHelper` to schedule a delayed task.
    - This task calls `NotifyPermissionSet` with a hardcoded `PermissionDecision::kDeny` value.
    - The delay is calculated as a random double between 1.0 and 2.0 seconds.
    - This is the practical implementation of the anti-fingerprinting defense: rather than an immediate, detectable rejection, the denial is asynchronous and mimics a user taking a moment to click "Block" on a prompt that never actually appears.

3.  **Android-Specific Logic**: For Android, there's a block that checks if the origin belongs to an installed WebAPK or Trusted Web Activity (TWA). If so, it delegates the permission decision to the Android-specific `InstalledWebappBridge`, which can interact with the Android OS-level notification settings for that app.

4.  **Default Path**: If none of the above conditions are met, the request is passed to the base class implementation, `ContentSettingPermissionContextBase::DecidePermission`. This is the standard path that leads to the `PermissionRequestManager` and the potential display of a user-facing prompt.