# SiteInfo

## 1. Component Focus
*   **Functionality:** Represents the security context of a web page or frame, used primarily for process allocation decisions. It encapsulates information derived from a URL, its origin, associated policies (sandboxing, isolation), and the browser context. A `SiteInfo` object determines which `SiteInstance` a document belongs to.
*   **Key Logic:** Determining the "site URL" (used for grouping same-site pages), determining the "process lock URL" (the security principal a process might be locked to), handling special URL schemes (`data:`, `about:blank`, `blob:`, `file:`, WebUI, error pages), incorporating isolation policies (isolated origins, OAC, COOP/COEP), sandboxing, guest status, and StoragePartition.
*   **Core Files:**
    *   `content/browser/site_info.h`
    *   `content/browser/site_info.cc`

## 2. Key Concepts & Interactions
*   **Site URL:** The URL representing the "site" (often scheme + eTLD+1) used for grouping pages within a `SiteInstance`. Can be affected by effective URL calculations (e.g., for hosted apps).
*   **Process Lock URL:** The security principal URL that a `RenderProcessHost` might be locked to. Often the same as the site URL, but can differ (e.g., for isolated origins, WebUI TLD grouping).
*   **UrlInfo:** A structure holding information about a URL needed to create a `SiteInfo`, including the URL itself, origin, sandboxing flags, isolation info (COOP/COEP, OAC), StoragePartition config, etc. Passed to `SiteInfo::Create`.
*   **IsolationContext:** Provides context about the `BrowsingInstance`, `BrowserContext`, guest status, etc., necessary for making isolation decisions when creating a `SiteInfo`.
*   **Origin Determination:** Crucial logic for handling non-standard schemes (`data:`, `about:blank`, `blob:`) and opaque origins, often involving inheritance from initiators or precursors. See Section 3.2.
*   **Requires Dedicated Process:** Logic (`SiteInfo::RequiresDedicatedProcess`) determining if this `SiteInfo` *must* run in a dedicated process based on flags, policies, or schemes.
*   **Should Lock Process:** Logic (`SiteInfo::ShouldLockProcessToSite`) determining if the process allocated for this `SiteInfo` should be strictly locked to its `process_lock_url`.

## 3. SiteInfo Creation (`SiteInfo::Create`, `SiteInfo::CreateInternal`)

The primary way `SiteInfo` objects are created, taking `IsolationContext` and `UrlInfo`.

**Steps (`CreateInternal`):**

1.  **Determine Process Lock URL:** Calls `DetermineProcessLockURL`. This typically involves calling `GetSiteForURLInternal` *without* using effective URLs. Special handling exists for WebUI URLs like `chrome://foo.bar` to lock based on TLD (`chrome://bar`).
2.  **Determine Site URL:** If `compute_site_url` is true (UI thread), calls `GetSiteForURLInternal` *with* effective URLs enabled. Otherwise, defaults to the `process_lock_url`.
3.  **Determine JIT/V8 Flags:** Checks policies based on the `process_lock_url`.
4.  **Determine StoragePartition:** Gets config based on `site_url` if not provided in `UrlInfo`.
5.  **Determine Isolation Info:** Calculates `WebExposedIsolationInfo` and `WebExposedIsolationLevel` based on `UrlInfo` and context.
6.  **Handle Error Pages:** If URL is `chrome-error:`, calls `CreateForErrorPage` which uses special site/lock URLs.
7.  **Determine Origin-Keyed Status:** Checks OAC policy (`DetermineOriginAgentClusterIsolation`) based on the URL's origin (handling overrides) to set `requires_origin_keyed_process_`.
8.  **Determine COOP Status:** Sets `does_site_request_dedicated_process_for_coop_` from `UrlInfo`.
9.  **Construct SiteInfo:** Creates the final object with all determined properties.

### 3.1 Site URL Calculation (`GetSiteForURLInternal`)

Calculates the site URL, potentially using effective URLs and handling special schemes.

**Logic:**

1.  Handles `chrome-error:` scheme first.
2.  Optionally gets effective URL.
3.  Determines the origin to use via `GetPossiblyOverriddenOriginFromUrl` (handles `data:`, `about:blank` overrides).
4.  **If Origin has Host (and not `file:`):**
    *   Checks Strict Origin Isolation policy.
    *   Checks Origin-Isolated Sandboxed Iframe policy.
    *   Defaults to site based on eTLD+1 (`GetSiteForOrigin`).
    *   Checks manually configured Isolated Origins (`GetMatchingProcessIsolatedOrigin`) and overrides if matched.
5.  **If Origin has No Host but has Scheme:**
    *   If non-opaque scheme, returns scheme (e.g., `file:`).
    *   If `data:` scheme: Returns precursor's site URL if sandboxed & origin-isolated, otherwise returns unique `data:<nonce>`.
    *   If `blob:` scheme: Returns full blob URL (minus hash) to isolate opaque origins.
    *   Otherwise, returns scheme (e.g., `about:`, `javascript:`).
6.  **Otherwise:** Returns empty GURL (invalid).

### 3.2 Origin Determination (`GetPossiblyOverriddenOriginFromUrl`)

Helper to get the correct `url::Origin` instance for site calculation, handling overrides.

**Logic:**

1.  Checks if scheme (`data:`, `about:blank`) allows override and `overridden_origin` is provided.
2.  If `data:`: Returns `overridden_origin`.
3.  If `about:blank`: Returns precursor origin if `overridden_origin` is opaque and precursor is valid. Otherwise resolves URL relative to `overridden_origin`.
4.  If no override applicable: Returns `url::Origin::Create(url)`.

## 4. Potential Logic Flaws & VRP Relevance
*   **Origin Confusion (javascript:/data:/blob:/about:blank):** VRPs (40059251, VRP2 173-1, 6260-1, 16487-1) suggest issues often arise when these schemes are used in specific contexts (new windows, redirects, iframes, session restore). The core logic might be correct in isolation, but the input `UrlInfo` (especially `origin` and precursor info) might be incorrect in these scenarios, leading to miscalculated `SiteInfo`. `javascript:` handling seems particularly underspecified here.
*   **Blob URL Registration Bypass:** VRP2 1761-1 indicates a compromised renderer might manipulate the origin associated with a blob URL *before* `SiteInfo` calculation, leading to `GetSiteForURLInternal` operating on incorrect data.
*   **Sandbox Inheritance:** VRP2 6777-1 highlights that navigating an iframe from `about:blank` to a sandboxed same-origin page can incorrectly reuse the non-sandboxed `about:blank` execution context. This implies `GetSiteForURLInternal` or related navigation logic might not correctly factor in sandbox flags during type system reuse checks (`FrameLoader::ShouldReuseDefaultView`).
*   **State Inconsistency:** VRP2 6260-1 (data URLs after restore) suggests state loss during serialization/deserialization (like tab restore) can lead to incorrect `SiteInfo` calculation later.

## 5. Areas Requiring Further Investigation
*   How `UrlInfo` (origin, precursor) is populated for `javascript:` and `data:` URLs opened in new windows/tabs, especially when initiated cross-origin.
*   Validation logic in `BlobURLRegistry::RegisterURL` against compromised renderer manipulation.
*   Interaction between `SiteInfo` calculation and `FrameLoader::ShouldReuseDefaultView`, specifically regarding iframe sandbox flags.
*   State preservation for data URLs and opaque origins during session restore.
*   How `javascript:` URLs inherit their execution context and origin, and whether `SiteInfo` calculation reflects this correctly in all scenarios.

*(See also: [url_info.md](url_info.md), [site_instance.md](site_instance.md), [browsing_instance.md](browsing_instance.md), [render_process_host.md](render_process_host.md), [navigation.md](navigation.md), [site_isolation.md](site_isolation.md), [process_lock.md](process_lock.md))*
