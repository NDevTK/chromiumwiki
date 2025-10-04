# The Chromium Permissions API Security Model

The Permissions API and the broader permission system in Chromium are fundamental to the web's security model. They act as the gatekeeper for powerful features, ensuring that websites cannot access sensitive user data or system resources without explicit, informed consent. This document synthesizes findings from code analysis to describe the security model that underpins this system.

## Core Components

The permission system is built on a few key components that work together to handle the lifecycle of a permission request.

1.  **`PermissionController`**: This is the central brain of the operation, residing in the browser process. Its implementation, `PermissionControllerImpl`, orchestrates the entire flow of a permission query or request. It is the single point of entry for permission checks originating from the renderer.

2.  **`PermissionControllerDelegate`**: An interface that abstracts away the platform-specific and UI-related aspects of permissions. This is what allows Chromium to have different permission prompt UIs on Android, Windows, and macOS. The delegate is responsible for actually showing a prompt to the user and relaying their decision.

3.  **`HostContentSettingsMap`**: This is the "database" for all content settings, including permissions. It's a profile-specific service that stores and retrieves settings based on a multi-layered provider architecture. It's the source of truth for whether a site has been granted a specific permission.

## A Permission's Lifecycle: The Security Model in Action

### 1. Querying the State (`navigator.permissions.query`)

A website can passively check the status of a permission (`granted`, `denied`, or `prompt`) without triggering a user prompt.

-   This request flows from the renderer to the `PermissionControllerImpl`.
-   The controller checks the `HostContentSettingsMap` for an existing setting for the site's origin.
-   **Security**: This is a safe, read-only operation. It allows sites to adapt their UI (e.g., show a "Enable Camera" button) without spamming the user with requests. The `denied` state is final unless the user manually changes it in settings, preventing sites from repeatedly asking for a permission the user has already refused.

### 2. Requesting a Permission (e.g., `geolocation.getCurrentPosition`)

This is an active request for a permission, which will likely trigger a user prompt if the status is currently `prompt`.

-   **Renderer to Browser**: The request is sent via an IPC call to the browser process.
-   **`PermissionControllerImpl` Validation**: The request is received by `PermissionControllerImpl`, which immediately performs several critical security checks:
    -   **Fenced Frames Check**: Requests originating from within a [fenced frame](https://developer.chrome.com/docs/privacy-sandbox/fenced-frame/) are **immediately denied**. This is a crucial defense to prevent untrusted, embedded content from gaining access to powerful APIs.
    -   **Permissions Policy Check**: The controller consults the document's [Permissions Policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Permissions_Policy). If the feature has been disabled by the top-level site (e.g., `geolocation 'none'`), the request is **immediately denied**. This empowers site owners to lock down their own applications.
    -   **Back-Forward Cache Check**: Requests from pages in the back-forward cache are rejected. This prevents pages that are not actively visible to the user from requesting permissions.
-   **Delegation**: If these initial checks pass, the request is handed off to the `PermissionControllerDelegate`.
-   **The User Prompt**: The delegate is responsible for displaying a prompt to the user. The design of this prompt is security-critical. It must clearly state which origin is requesting which permission, preventing spoofing and ensuring informed consent.

### 3. Storing the Decision

-   Once the user makes a choice (Allow or Block), the delegate communicates this back to the `PermissionControllerImpl`.
-   The controller then instructs the `HostContentSettingsMap` to persist this decision.
-   The `PrefProvider` within the map writes the setting to the user's profile preferences, associating the permission with the requesting origin (and potentially the top-level origin).

### 4. Enforcing the Decision

-   On subsequent attempts by the website to use the feature, the check again goes to the `PermissionControllerImpl`.
-   This time, when it queries the `HostContentSettingsMap`, the `PrefProvider` will find the user's stored decision (`Allow` or `Block`).
-   **Provider Precedence**: This check respects the strict provider hierarchy. If an enterprise policy exists in the `PolicyProvider` (e.g., blocking the camera for all sites), it will be returned first, and the user's preference will be ignored. This ensures administrators can enforce a security baseline.
-   The final decision is returned to the renderer, granting or denying access to the feature.

## Key Security Principles

-   **User Consent is Paramount**: The default state for most powerful permissions is `prompt`. Nothing is granted by default; the user is the ultimate authority.
-   **Origin-Based Trust**: Permissions are granted to specific origins. `https://good.example` having camera access does not grant it to `https://evil.example`.
-   **Secure Contexts**: Many sensitive APIs (like Geolocation, Camera, Microphone) are only available to secure origins (HTTPS). This prevents network attackers from intercepting and manipulating the feature.
-   **Clear Revocation**: Users can always view and revoke permissions through the browser's settings UI, ensuring they remain in control.
-   **Layered Defenses**: The security model doesn't rely on a single check. It's a chain of defenses: Permissions Policy -> Fenced Frame checks -> User Prompts -> Stored Settings.
-   **Ephemeral Grants**: The system supports temporary, one-time permissions, which automatically expire. This provides a "just-in-time" access model that enhances privacy by avoiding persistent grants for one-off tasks. The `HostContentSettingsMap` is responsible for tracking and expiring these grants.