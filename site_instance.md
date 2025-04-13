# SiteInstance

## 1. Component Focus
*   **Functionality:** Represents an instance of a specific "site" within a `BrowsingInstance`. A `SiteInstance` is typically associated with a single renderer process (via `SiteInstanceGroup`), although multiple `SiteInstance`s might share a process under certain conditions (e.g., default SiteInstance). It plays a crucial role in enforcing site isolation by ensuring content from different sites does not end up in the same process unless explicitly allowed.
*   **Key Logic:** Determining the site (`SiteInfo`) associated with a URL, managing the lifecycle and process assignment (`GetOrCreateProcess`, `SetProcessInternal`), checking suitability for URLs (`IsSuitableForUrlInfo`), handling process reuse policies (`ProcessReusePolicy`), managing relation to `BrowsingInstance` and `SiteInstanceGroup`, determining if a site can use the default shared instance (`CanBePlacedInDefaultSiteInstance`).
*   **Core Files:**
    *   `content/browser/site_instance_impl.cc`/`.h`
    *   `content/browser/browsing_instance.cc`/`.h`: Manages the collection of `SiteInstance`s. Contains `GetSiteInstanceForURL` which finds/creates SiteInstances *within* a BrowsingInstance.
    *   `content/browser/renderer_host/render_frame_host_manager.cc`: Manages RFHs within a FrameTreeNode. Contains `GetSiteInstanceForNavigation` which determines the target SiteInstance for a navigation, potentially spanning BrowsingInstances.
    *   `content/browser/site_info.cc`/`.h`: Defines the "site" based on URL, policies, etc. Critical input for SiteInstance decisions.
    *   `content/browser/site_instance_group.cc`/`.h`: Groups SiteInstances sharing a process/AgentSchedulingGroup.
    *   `content/public/browser/site_instance.h` (Public API)
    *   `content/browser/renderer_host/render_process_host_impl.cc`/`.h`: Implements `IsSuitableHost`.
    *   `content/browser/process_reuse_policy.h`: Defines process reuse policies.

## 2. Key Concepts & Interactions
*   **BrowsingInstance:** Owns a set of `SiteInstance`s that can script each other (more or less). A `BrowsingInstance` swap usually means a loss of script connection. `BrowsingInstance::GetSiteInstanceForURL` is the central function for finding/creating `SiteInstance`s *within* the current `BrowsingInstance`.
*   **SiteInfo:** Represents the security principal (the "site") associated with a URL. See [site_info.md](site_info.md).
*   **UrlInfo:** Packages a URL with additional contextual security information. Input to `SiteInfo::Create`. See [url_info.md](url_info.md).
*   **Default SiteInstance (`default_site_instance_`):** A potentially shared `SiteInstance` within the `BrowsingInstance` used for sites that *do not* require a dedicated process.
*   **SiteInstance Relation:** During navigation, the target `SiteInstance` can have different relationships to the current one, influencing process selection.
*   **Process Assignment:** `SiteInstanceImpl` usually gets associated with a `RenderProcessHost` via `SiteInstanceGroup`. `SiteInstanceImpl::GetOrCreateProcess` handles this, potentially reusing processes based on `RenderProcessHostImpl::MayReuseAndIsSuitable` and `ProcessReusePolicy`.
*   **Process Locking:** `SiteInstanceImpl::LockProcessIfNeeded` ensures the associated `RenderProcessHost` is locked if needed.
*   **Process Reuse Policy (`process_reuse_policy_`):** An enum (`content::ProcessReusePolicy`) set on the `SiteInstance` that influences how `RenderProcessHostImpl::GetProcessHostForSiteInstance` searches for an existing process to reuse. See Section 6 below.

## 3. SiteInstance Selection During Navigation

Occurs primarily within `RenderFrameHostManager::GetSiteInstanceForNavigation`.
1.  Determines if a `BrowsingInstance` swap is needed (`ShouldSwapBrowsingInstancesForNavigation`).
2.  Determines the required `SiteInstance` type/relation (`DetermineSiteInstanceForURL`).
3.  Resolves to a concrete `SiteInstanceImpl` (`ConvertToSiteInstance`), checking suitability (`IsSuitableForUrlInfo`).
4.  Applies `ProcessReusePolicy` based on context (e.g., subframe, main frame threshold).
5.  Attempts process reuse heuristics.

*(See [navigation.md](navigation.md) for more details)*

## 4. Suitability Check (`SiteInstanceImpl::IsSuitableForUrlInfo`)

Checks if the *current* `SiteInstanceImpl` object (`this`) is appropriate for hosting the target `url_info`. Called during navigation (e.g., by `RenderFrameHostManager`) to check if an existing SI can be reused.

**Logic Summary:**

*   **Returns `true` (Suitable) if:** URL is debug/`about:blank` (and SI not error page); SI is unassigned and has no process; SI has no process but matches `SiteInfo` (and not guest); SI has no process, `SiteInfo`s differ, but neither requires dedicated process.
*   **Returns `false` (Unsuitable) if:** Sandbox mismatch; Current SI is default but URL needs specific SI; SI has no process, `SiteInfo`s differ, and one requires dedicated process.
*   **Delegates to `RenderProcessHostImpl::IsSuitableHost` if:** SI has a process. Checks if the *existing process* and its lock are compatible with the target `url_info`'s derived `SiteInfo`.

*(See [render_process_host.md](render_process_host.md) for `IsSuitableHost` details)*

## 5. Default SiteInstance Eligibility (`CanBePlacedInDefaultSiteInstance`)

Static method determining if a URL/SiteInfo can share the process associated with the `BrowsingInstance`'s default `SiteInstance`. Called by `BrowsingInstance::GetSiteInstanceForURLHelper`.

**Logic:** Returns `false` (cannot use default) if: URL is `file:`, URL doesn't assign a site (initial `about:blank`), or `SiteInfo::RequiresDedicatedProcess` returns `true`. Otherwise `true`.

## 6. Process Reuse Policy (`content::ProcessReusePolicy`)

This enum, stored in `SiteInstanceImpl::process_reuse_policy_`, guides `RenderProcessHostImpl::GetProcessHostForSiteInstance` when searching for a reusable process.

*   **`PROCESS_PER_SITE`:** Enforces that all instances of the site share a single designated process within the `BrowserContext`. Uses `RenderProcessHostImpl::GetSoleProcessHostForSite`.
*   **`REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME`:** For subframes. Tries to reuse a process already hosting a pending/committed frame for the same site. Prioritizes foreground processes. Uses `FindReusableProcessHostForSiteInstance`.
*   **`REUSE_PENDING_OR_COMMITTED_SITE_WORKER`:** Similar, but specifically for workers. Reuse decisions might differ slightly. Uses `FindReusableProcessHostForSiteInstance`.
*   **`REUSE_PENDING_OR_COMMITTED_SITE_WITH_MAIN_FRAME_THRESHOLD`:** For main frames (under feature flag). Similar reuse logic but also checks resource thresholds (`IsBelowReuseResourceThresholds`). Uses `FindReusableProcessHostForSiteInstance`.
*   **`DEFAULT`:** No proactive reuse based on pending/committed sites. May reuse a process with an *unmatched* service worker for the site (via `UnmatchedServiceWorkerProcessTracker::MatchWithSite`, only for navigations). Otherwise, generally creates a new process unless the process limit is hit, then uses random suitable process reuse (`GetExistingProcessHost`).

*(See [render_process_host.md](render_process_host.md) for details on the process selection functions mentioned above)*

## 7. Potential Logic Flaws & VRP Relevance (Summary - See site_isolation.md for details)
*   **Incorrect Suitability Check (`IsSuitableForUrlInfo`)**: Reusing an SI incorrectly.
*   **Incorrect Site/Origin Determination (in `SiteInfo::Create`)**: Leads to wrong `SiteInstance` selection.
*   **Incorrect Default SiteInstance Usage (in `CanBePlacedInDefaultSiteInstance`)**: Placing an isolated site into the default process.
*   **Process Allocation/Reuse Errors (in `GetOrCreateProcess`, `MayReuseAndIsSuitable`, `GetProcessHostForSiteInstance` based on policy)**: Sharing processes inappropriately.
*   **Process Lock Bypass/Errors (in `LockProcessIfNeeded`, `SetProcessLock`)**: Allowing wrong content into a locked process.
*   **Navigation/Redirect Flaws**: State confusion affecting `SiteInstance` selection during navigation.

## 8. Functions and Methods (Key Ones for Isolation Logic)
*   **`BrowsingInstance::GetSiteInstanceForURL`**: Finds/creates SI *within* a BI.
*   **`RenderFrameHostManager::GetSiteInstanceForNavigation`**: Determines target SI for navigation, considering BI swaps.
*   **`SiteInfo::Create`**: Determines site properties from `UrlInfo`.
*   **`SiteInfo::RequiresDedicatedProcess`**: Checks if site needs dedicated process.
*   **`SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`**: Checks eligibility for default SI.
*   **`SiteInstanceImpl::IsSuitableForUrlInfo`**: Checks if *this* SI can host a target URL.
*   **`RenderProcessHostImpl::IsSuitableHost`**: Checks if an *existing process* can host a target `SiteInfo`.
*   **`RenderProcessHostImpl::GetProcessHostForSiteInstance`**: Selects/creates process based on `ProcessReusePolicy`.
*   **`SiteInstanceImpl::LockProcessIfNeeded`**: Applies process lock.
*   **`ChildProcessSecurityPolicyImpl::IsIsolatedOrigin` / `DetermineOriginAgentClusterIsolation`**: Policy/OAC checks.

## 9. Areas Requiring Further Investigation (Highlights)
*   Detailed analysis of `RenderProcessHostImpl::IsSuitableHost` and `RenderProcessHostImpl::FindReusableProcessHostForSiteInstance`.
*   Logic within `IsBelowReuseResourceThresholds` for the main frame threshold policy.
*   The workings of `UnmatchedServiceWorkerProcessTracker`.
*   How `SiteInstanceImpl::process_reuse_policy_` gets set (e.g., in `GetSiteInstanceForNavigation`).

## 10. Related VRP Reports
*(See [site_isolation.md](site_isolation.md) for a comprehensive list)*

*(See also: [site_info.md](site_info.md), [url_info.md](url_info.md), [browsing_instance.md](browsing_instance.md), [site_instance_group.md](site_instance_group.md), [site_isolation.md](site_isolation.md), [render_process_host.md](render_process_host.md), [navigation.md](navigation.md), [navigation_request.md](navigation_request.md), [process_lock.md](process_lock.md))*
