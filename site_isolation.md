# Site Isolation & Process Model

## 1. Component Focus
*   **Functionality:** Encompasses the core mechanisms Chromium uses to separate websites into different processes to mitigate security risks like Spectre and improve robustness. This includes determining site boundaries, assigning sites to processes, managing process locks, and handling exceptions and policies related to isolation.
*   **Key Logic:** Site definition (`SiteInfo`), SiteInstance assignment (`BrowsingInstance`, `SiteInstanceImpl`, `RenderFrameHostManager`), process allocation (`RenderProcessHostImpl`), process locking (`ProcessLock`), handling of isolated origins and agent clusters (OAC), COOP/COEP enforcement.
*   **Core Files:**
    *   `content/browser/site_info.*`
    *   `content/browser/site_instance_impl.*`
    *   `content/browser/browsing_instance.*`
    *   `content/browser/site_instance_group.*`
    *   `content/browser/renderer_host/render_frame_host_manager.*`
    *   `content/browser/renderer_host/render_process_host_impl.*`
    *   `content/browser/child_process_security_policy_impl.*`
    *   `content/public/browser/site_isolation_policy.*`

## 2. Key Concepts
*   **Site:** A security principal, typically scheme + eTLD+1 (e.g., `https://google.com`), but can be finer-grained (origin-level) for isolated origins or OAC. Defined by `SiteInfo`.
*   **SiteInstance:** Represents a specific site within a `BrowsingInstance`. Typically maps 1:1 to a `SiteInstanceGroup`. See [site_instance.md](site_instance.md).
*   **BrowsingInstance:** A collection of frames/tabs that can potentially script each other. See [browsing_instance.md](browsing_instance.md).
*   **SiteInstanceGroup:** A collection of `SiteInstance`s that share a `RenderProcessHost` and `AgentSchedulingGroupHost`. See [site_instance_group.md](site_instance_group.md).
*   **Process Lock:** A security property associated with a `RenderProcessHost`, restricting which sites/origins it can host. See [process_lock.md](process_lock.md).
*   **Isolated Origins:** Origins explicitly configured (via policy or command-line) to always require a dedicated process, potentially isolating them more strictly than just by site.
*   **Origin-Agent-Cluster (OAC):** A header (`Origin-Agent-Cluster: ?1`) allowing origins to request logical and potentially process isolation based on origin rather than site.
*   **COOP/COEP:** Headers (`Cross-Origin-Opener-Policy`, `Cross-Origin-Embedder-Policy`) enabling cross-origin isolation, unlocking features like `SharedArrayBuffer`. Can influence process model decisions (e.g., requiring BrowsingInstance swaps). See [coop_coep.md](coop_coep.md).

## 3. Core Isolation Logic

### 3.1. Does a Site Require a Dedicated Process? (`SiteInfo::RequiresDedicatedProcess`)

This function determines if a given `SiteInfo` *must* be placed in a dedicated process, preventing it from sharing the default process.

**Returns `true` (Dedicated Process Required) if:**

1.  **Global Site Isolation:** `--site-per-process` flag is enabled.
2.  **COOP Request:** `SiteInfo::does_site_request_dedicated_process_for_coop_` flag is true (set based on COOP header + eligibility).
3.  **Isolated Origin Policy:** The origin/site matches an entry configured via policy or command-line (`ChildProcessSecurityPolicyImpl::IsIsolatedOrigin`). See Section 3.2 below.
4.  **Sandboxed Frame:** `SiteInfo::is_sandboxed_` flag is true.
5.  **Error Page:** `SiteInfo::is_error_page()` returns true.
6.  **PDF Content:** `SiteInfo::is_pdf_` flag is true.
7.  **WebUI:** `SiteInfo::site_url_` scheme is a registered WebUI scheme.
8.  **Embedder Policy:** `ContentBrowserClient::DoesSiteRequireDedicatedProcess` returns true.

If none apply, returns `false`. See [site_info.md](site_info.md) for more details.

### 3.2. Is an Origin Explicitly Isolated? (`ChildProcessSecurityPolicyImpl::IsIsolatedOrigin`)

This checks if an `origin` should be isolated based on manually configured policies (enterprise policy or `--isolate-origins` command-line flag), potentially considering Origin-Agent-Cluster (OAC) status. It delegates to `GetMatchingProcessIsolatedOrigin`.

**Logic (`GetMatchingProcessIsolatedOrigin`):**

1.  **OAC Check:** If process isolation for OAC is enabled (`SiteIsolationPolicy::IsProcessIsolationForOriginAgentClusterEnabled()`):
    *   Calls `DetermineOriginAgentClusterIsolation` to check if the `origin`, given its requested OAC state (`requests_origin_keyed_process`) and the `IsolationContext`, requires an origin-keyed process.
    *   If yes: Returns `true` (origin is isolated).
2.  **Policy/Cmd-Line Check:** If OAC doesn't mandate isolation:
    *   Looks up isolated origin patterns matching the `origin`'s site URL in the `isolated_origins_` map.
    *   Filters patterns based on applicability to the current profile (`MatchesProfile`) and BrowsingInstance timeline (`MatchesBrowsingInstance`).
    *   Checks if the specific `origin` matches any applicable pattern (exact or subdomain via `IsolatedOriginUtil::DoesOriginMatchIsolatedOrigin`).
    *   If a match is found: Returns `true` (origin is isolated).
3.  **Default:** Returns `false`.

### 3.3. Can Site Use Default Process? (`SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`)

Checks if a URL/SiteInfo can use the shared default `SiteInstance`.

**Returns `false` (Cannot use default) if:**

1.  URL scheme is `file:`.
2.  URL does not assign a site (e.g., initial `about:blank`).
3.  `SiteInfo::RequiresDedicatedProcess` returns `true` for the site (see Section 3.1).

Otherwise, returns `true`. See [site_instance.md](site_instance.md) for more details.

### 3.4. SiteInstance Selection

Combines the above checks during navigation:
*   `RenderFrameHostManager` decides if a `BrowsingInstance` swap is needed based on security/policy/heuristics (`ShouldSwapBrowsingInstancesForNavigation`).
*   `RenderFrameHostManager` determines the required `SiteInstance` type (relation to current) based on the swap decision and context (`DetermineSiteInstanceForURL`).
*   `RenderFrameHostManager` resolves the type into a concrete `SiteInstance`, potentially creating one (`ConvertToSiteInstance`).
*   If creating/finding an SI *within* a `BrowsingInstance`, `BrowsingInstance::GetSiteInstanceForURL` uses `CanBePlacedInDefaultSiteInstance` to decide between using the default SI or a specific SI (found in map or newly created).

See [navigation.md](navigation.md), [site_instance.md](site_instance.md), and [browsing_instance.md](browsing_instance.md) for the detailed flow.

## 4. Potential Logic Flaws & VRP Relevance
*   **Incorrect Site/Origin Classification:** Errors in `SiteInfo::Create` or helpers like `GetSiteForURLInternal` can lead to misclassifying a site, resulting in incorrect process placement.
    *   **VRP 40059251:** Browser-side origin confusion for javascript/data URLs opened in a new window/tab by cross-origin iframe.
*   **Isolation Bypass via Default SI:** Flaws in `SiteInfo::RequiresDedicatedProcess` or `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance` allowing a site that *should* be isolated to share the default process.
*   **Policy Bypass:** Incorrectly evaluating command-line flags (`--site-per-process`) or enterprise policies (`IsIsolatedOrigin`) leading to insufficient isolation.
*   **OAC/COOP/Sandboxing Misconfiguration:** Bugs in how `UrlInfo` represents these states or how `SiteInfo` interprets them can break expected isolation boundaries.
    *   **VRP 40056434:** crossOriginIsolated bypass (related to COOP/COEP enforcement).
*   **Process Lock Errors:** Mistakes in `ShouldLockProcessToSite` or applying the lock in `LockProcessIfNeeded` could fail to properly restrict a process. Incorrect comparisons in `IsSuitableHost` / `ProcessLock::operator==`.
*   **WebUI Privilege Escalation:** Incorrect process reuse allowing non-WebUI sites to gain WebUI privileges.
    *   **VRP 40053875:** Some WebUI pages enable MojoJS bindings for the subsequently-navigated site.
*   **State Confusion during Navigation/Redirects:** Complex navigations might lead to inconsistent state being used for isolation decisions at different stages.
*   **Information Leaks Across BrowsingInstances:** Incorrect logic in `ShouldSwapBrowsingInstancesForNavigation` or proxy handling might leak information (like `window.length`).
    *   **VRP 40059056:** Leaking window.length without opener reference.
*   **Post-Crash State Issues:** Incorrect handling of state after a renderer crash leading to potential spoofs or bypasses.
    *   **VRP 40057561:** URL Spoof after crash.

## 5. Areas Requiring Further Investigation
*   Detailed analysis of `ChildProcessSecurityPolicyImpl::DetermineOriginAgentClusterIsolation`.
*   Audit conditions checked by `ContentBrowserClient` callbacks (`DoesSiteRequireDedicatedProcess`, `ShouldLockProcessToSite`).
*   Interaction between `IsIsolatedOrigin` (policy) and `requires_origin_keyed_process_` (OAC header) in `SiteInfo::RequiresDedicatedProcess`.
*   Handling of edge case URL schemes (`file:`, `data:`, `blob:`, `javascript:`, `about:`) in all relevant functions, especially concerning initiator origins and new windows.
*   State management and process selection logic during renderer crash recovery.
*   Thorough review of `ShouldSwapBrowsingInstancesForNavigation` conditions, particularly proactive swaps and COOP/COEP interactions.

## 6. Related VRP Reports (Examples)
*   **40059251:** Browser-side origin confusion for javascript/data URLs.
*   **40053875:** WebUI MojoJS bindings leak to subsequent sites.
*   **40056434:** crossOriginIsolated bypass.
*   **40059056:** window.length leak without opener.
*   **40057561:** URL Spoof after crash.

*(See also: [site_info.md](site_info.md), [url_info.md](url_info.md), [site_instance.md](site_instance.md), [browsing_instance.md](browsing_instance.md), [render_process_host.md](render_process_host.md), [navigation.md](navigation.md), [coop_coep.md](coop_coep.md), [process_lock.md](process_lock.md))*
