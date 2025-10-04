# The Top-Level Storage Access API Security Model

The `TopLevelStorageAccessPermissionContext` is the specialized handler for the Top-Level Storage Access API. This API is a modern privacy feature designed to allow top-level sites to request storage access on behalf of embedded third-party origins, which would otherwise have their cookie access blocked under modern privacy policies (like blocking all third-party cookies).

The header file `chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h` reveals a security model that is significantly more nuanced than a simple allow/block permission. It's built around the principle of **mediated access** and relies heavily on the concept of **First-Party Sets**.

## Core Concepts

### 1. First-Party Sets (FPS)

-   The primary mechanism for granting this permission is not a user prompt, but an automatic check against First-Party Sets metadata.
-   A First-Party Set is a list of domains that are owned and operated by the same entity. By declaring this relationship, sites can signal to the browser that they are part of the same "privacy boundary."
-   The `TopLevelStorageAccessPermissionContext` uses this information to **auto-grant** requests where the top-level site and the embedded site are both members of the same First-Party Set. This provides a seamless user experience for related sites without compromising the browser's default third-party cookie blocking.

### 2. Automatic Decisions (No Prompt)

-   The `CheckForAutoGrantOrAutoDenial` method is a key part of the flow. Before ever considering a user prompt, this method is called to see if an automatic decision can be made.
-   **Auto-Grant**: Occurs if the sites are in the same First-Party Set.
-   **Auto-Denial**: Occurs if the sites are *not* in the same First-Party Set, or if other prerequisites are not met.

### 3. Link to Cookie Settings

-   The `TopLevelStorageAccessRequestOutcome` enum includes `kDeniedByCookieSettings`. This indicates that the permission is subservient to the user's global cookie settings.
-   If a user has configured Chrome to block all third-party cookies, this API cannot be used to bypass that setting. The request will be automatically denied. This ensures that the user's explicit privacy choices are always respected.

### 4. Prerequisites for Requesting

-   The outcome `kDeniedByPrerequisites` suggests that, like many powerful APIs, a request for top-level storage access requires certain preconditions to be met. These typically include:
    -   The request must be triggered by a **user gesture** (e.g., a click). This prevents sites from programmatically requesting access without user interaction.
    -   The request must originate from a secure context (HTTPS).

## Security Implications

-   **Scoped by Default**: This API is designed to be a specific, intentional exception to the default third-party cookie blocking. It doesn't grant broad access; it grants access for a specific top-level site and a specific embedded site.
-   **Reduced Prompt Fatigue**: By relying on First-Party Sets for auto-grants, the API avoids prompting the user for every interaction between related sites, which improves usability and reduces the risk of users becoming "prompt-blind" and simply clicking "Allow" on everything.
-   **Developer-Declared Trust**: The First-Party Sets model shifts some of the burden of trust from the user to the developer. Developers must proactively declare their related domains, which is a public and auditable action.
-   **Respects User's Global Settings**: The fact that the API honors the user's global cookie settings is a critical fallback. It ensures that even if a site is part of a First-Party Set, it cannot gain storage access if the user has made a global decision to block it.

In summary, the `TopLevelStorageAccessPermissionContext` implements a sophisticated, policy-driven security model. It prioritizes automatic, privacy-preserving decisions based on developer-declared relationships (First-Party Sets) and user's global settings, resorting to a direct user prompt only when necessary. This is a significant departure from the simple allow/deny model of many older permissions.

## Implementation Details

The implementation in `top_level_storage_access_permission_context.cc` reveals the precise order of operations for making a decision.

### The `DecidePermission` Flow

1.  **Prerequisite Checks**: The method first validates that the request is coming from the primary main frame and is associated with a user gesture. If not, the request is immediately denied with a console message. This is a critical first line of defense against abuse.

2.  **Asynchronous FPS Lookup**: If the prerequisites are met, the context does not proceed synchronously. It calls `first_party_sets_policy_service->ComputeFirstPartySetMetadata`. This is an asynchronous call to check the relationship between the requesting origin and the top-level origin. The rest of the logic is deferred to the callback, `CheckForAutoGrantOrAutoDenial`.

### The `CheckForAutoGrantOrAutoDenial` Callback

This method contains the core decision-making logic:

1.  **Same Set Check**: It checks the result of the FPS lookup (`metadata.AreSitesInSameFirstPartySet()`).

2.  **Auto-Grant Path**: If the sites **are** in the same First-Party Set, it performs two more crucial checks before granting:
    -   It ensures the top-level site is not a "service" domain (`metadata.top_frame_entry()->site_type() != net::SiteType::kService`). Service domains in a set cannot request access on behalf of others.
    -   It checks the user's global cookie settings (`settings_map->GetContentSetting(...)`). If the user has explicitly set `CONTENT_SETTING_BLOCK` for cookies between these two sites, the request is **denied**. This ensures the user's explicit choice is always honored over the FPS policy.
    -   If these checks pass, the permission is **auto-granted**.

3.  **Auto-Denial Path**: If the sites **are not** in the same First-Party Set, the request is **auto-denied**.

### Granting and Storing the Permission

When a grant occurs, `NotifyPermissionSetInternal` is called:

-   **Temporary Grant**: The permission is not granted forever. It is stored with a specific lifetime (`kStorageAccessAPIRelatedWebsiteSetsLifetime`), after which it expires.
-   **Dual Grant**: A grant for `TOP_LEVEL_STORAGE_ACCESS` also creates a grant for the regular `STORAGE_ACCESS` permission.
-   **Network Service Update**: The context uses a `BarrierClosure` to update the cookie settings in the Network Service *before* notifying the renderer. This is a critical step to prevent race conditions where the page might try to make a network request with cookies before the network process is aware that it's allowed to.

### Querying Status: Hiding Denials

The `GetContentSettingStatusInternal` method has a unique privacy feature. If it finds that a permission has been explicitly set to `CONTENT_SETTING_BLOCK`, it reports the status as `CONTENT_SETTING_ASK`. This prevents a site from using the permission status to fingerprint or retaliate against a user who has denied a request.