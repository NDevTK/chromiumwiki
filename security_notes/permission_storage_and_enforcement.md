# Permission Storage and Enforcement in Chromium

This document details how Chromium stores and enforces permission decisions. The primary component responsible for this is the `HostContentSettingsMap`.

## `HostContentSettingsMap`: The Core of Permission Storage

The `HostContentSettingsMap` class is the central authority for managing all content settings, including permissions. It is a profile-specific service that provides a unified view of settings from various sources.

-   **Path**: `components/content_settings/core/browser/host_content_settings_map.h`, `components/content_settings/core/browser/host_content_settings_map.cc`

### Key Characteristics:

-   **Profile-Bound**: Each browser profile (including incognito profiles) has its own instance of `HostContentSettingsMap`.
-   **Provider-Based Architecture**: The map doesn't store settings directly. Instead, it aggregates them from a series of "providers," each responsible for a different source of settings. This allows for a clear separation of concerns and a well-defined precedence order.

### The Provider System

The `HostContentSettingsMap` uses a layered system of providers to determine the setting for a given request. The providers are queried in a specific order of precedence:

1.  **PolicyProvider**: Enforces settings from enterprise policies. These have the highest precedence and cannot be overridden by the user.
2.  **PrefProvider**: Manages user-configured settings. This is where most user-granted or user-denied permissions are stored, persisted in the user's profile via `PrefService`.
3.  **DefaultProvider**: Provides the default setting for each content type. This is the fallback when no other provider has a specific rule.

Other providers can be registered for things like extensions, which can also influence content settings.

## How Permissions Are Stored

-   **User Preferences**: User decisions (Allow, Block, etc.) are typically handled by the `PrefProvider`, which serializes the settings and stores them in the user's profile preferences files.
-   **Pattern-Based Rules**: Settings are not just stored for a single origin. They are associated with a `ContentSettingsPattern` pair:
    -   **Primary Pattern**: Usually represents the origin requesting the permission (the "requesting origin").
    -   **Secondary Pattern**: Often represents the origin of the top-level page (the "embedding origin"). A wildcard (`*`) is used when the setting applies regardless of the embedding context.
-   **In-Memory Cache**: The `HostContentSettingsMap` maintains an in-memory representation of the settings for quick access.

## How Permissions Are Enforced

The enforcement process begins when a feature needs to check if it has permission to proceed. This is done by querying the `HostContentSettingsMap`.

### The `GetWebsiteSetting` Method

The primary method for checking a permission is `HostContentSettingsMap::GetWebsiteSetting`.

1.  **Input**: The method takes the primary URL (requesting origin), the secondary URL (embedding origin), and the `ContentSettingsType` (e.g., `GEOLOCATION`, `NOTIFICATIONS`).

2.  **Provider Iteration**: It iterates through its list of providers in their order of precedence.

3.  **Rule Matching**: For each provider, it checks if there is a rule that matches the given URLs and content type.

4.  **First Match Wins**: The first provider in the list that has a matching rule determines the outcome. For example, if a policy is set to block geolocation, the `PolicyProvider` will return a "Block" setting, and the process stops there. The user's own setting in the `PrefProvider` will not even be considered.

5.  **Incognito Inheritance**: If the check is for an incognito profile, the `HostContentSettingsMap` applies special inheritance rules. Some permissions are inherited from the main profile (e.g., a "Block" setting), while others are not, forcing a new prompt in incognito mode. This logic is handled by `ProcessIncognitoInheritanceBehavior`.

6.  **Default Value**: If no provider has a specific rule, the `DefaultProvider` supplies the default setting for that permission type (e.g., "Ask").

### Security-Critical Aspects

-   **Precedence Order**: The strict precedence of providers is a critical security feature. It ensures that enterprise policies and other high-priority settings cannot be accidentally or maliciously overridden by lower-priority sources like user preferences.
-   **Pattern Specificity**: The use of primary and secondary patterns allows for nuanced rules. For instance, a user can grant a permission to an embedded iframe from `service.com` only when it's embedded in `mainsite.com`, but not when it's embedded elsewhere.
-   **Ephemeral Permissions**: The map supports one-time permissions that expire after a certain period or when the tab is closed. The `DeleteNearlyExpiredSettingsAndMaybeScheduleNextRun` function handles the logic for cleaning up these expired permissions.

This system provides a robust and flexible framework for managing permissions, balancing user control with administrative policies and the security needs of the browser.