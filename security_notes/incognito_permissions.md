# Permissions in Incognito Mode

Chromium's Incognito mode has a distinct security and privacy model, and this extends to how it handles website permissions. The goal is to provide a temporary, ephemeral browsing session that leaves no local traces. This principle fundamentally changes how permission grants are treated. The core logic for this is found in the `ProcessIncognitoInheritanceBehavior` function within `host_content_settings_map.cc`.

## The Principle of Ephemeral Sessions

Unlike a regular profile, where a permission grant is persistent, any permission granted in an Incognito session is **ephemeral**. It lasts only for the duration of that session. Once all Incognito windows are closed, these permissions are discarded. This is a core privacy feature.

## A Delegated Inheritance Model

When an incognito profile is created, it doesn't start with a blank slate. It inherits content settings from the regular profile it was created from. However, not all settings are inherited equally. The `HostContentSettingsMap` for the incognito profile uses the `ProcessIncognitoInheritanceBehavior` function to decide what to do with a setting from the main profile.

The key insight is that this logic is **delegated**. The `HostContentSettingsMap` does not contain a master list of how to handle each permission. Instead, it queries the metadata for each specific permission type.

### `ProcessIncognitoInheritanceBehavior`

This function performs two main checks:

1.  **Blanket Disallowance**: It first checks the `WebsiteSettingsInfo` for the permission. If the metadata includes the flag `DONT_INHERIT_IN_INCOGNITO`, the setting is completely discarded. The function returns an empty value, meaning the incognito profile will not inherit this setting under any circumstances.

2.  **Context-Specific Logic**: If the setting is inheritable, the function calls `permission_info->delegate().InheritInIncognito(setting)`. This passes the decision to the specific `PermissionContext` responsible for that permission type.

## Common Inheritance Rules

While the logic is delegated, a common pattern emerges from the various permission context implementations:

-   **`ALLOW` is generally not inherited**: If a user has permanently granted a permission to a site in their regular profile, that grant is typically **not** carried over to Incognito mode. The site must ask for the permission again. This upholds the principle that Incognito is a separate, temporary context. Granting a permission in Incognito does not affect the regular profile.
-   **`BLOCK` is generally inherited**: If a user has permanently blocked a permission for a site, that block **is** inherited by the Incognito profile. This is a security and anti-annoyance feature. The user has made a clear decision to not trust a site with a permission, and that decision should be respected even in a temporary session.
-   **`ASK` remains `ASK`**: The default state for most permissions is to prompt the user, and this behavior is inherited.

## Security and Privacy Implications

-   **Prevents Tracking via Permissions**: By not inheriting `ALLOW` grants, Incognito mode prevents sites from using previously granted permissions as a tracking vector to identify a user across regular and private browsing sessions.
-   **User Control and Expectations**: The model aligns with user expectations for private browsing. Users expect a more locked-down, temporary session. Forcing sites to re-request permissions reinforces that the session is isolated and ephemeral.
-   **Persistent Mistrust**: Inheriting `BLOCK` decisions is a critical security measure. It ensures that sites a user has actively chosen to distrust cannot bypass that decision simply by being opened in an Incognito window.
-   **Ephemeral Grants**: Any permission granted within an Incognito window is written only to the in-memory `HostContentSettingsMap` for that "off-the-record" profile. It is never persisted to disk and is destroyed when the last Incognito window is closed.