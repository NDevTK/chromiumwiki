# SiteInstance

## 1. Component Focus
*   **Functionality:** Represents an instance of a specific "site" within a `BrowsingInstance`. A `SiteInstance` is typically associated with a single renderer process (via `SiteInstanceGroup`), although multiple `SiteInstance`s might share a process under certain conditions (e.g., default SiteInstance). It plays a crucial role in enforcing site isolation by ensuring content from different sites does not end up in the same process unless explicitly allowed.
*   **Key Logic:** Determining the site (`SiteInfo`) associated with a URL, managing the lifecycle and process assignment (`GetOrCreateProcess`, `SetProcessInternal`), checking suitability for URLs (`IsSuitableForUrlInfo`), handling process reuse policies (`ProcessReusePolicy`), managing relation to `BrowsingInstance` and `SiteInstanceGroup`.
*   **Core Files:**
    *   `content/browser/site_instance_impl.cc`/`.h`
    *   `content/browser/browsing_instance.cc`/`.h`: Manages the collection of `SiteInstance`s and contains the key `GetSiteInstanceForURL` logic.
    *   `content/browser/site_info.cc`/`.h`: Defines the "site" based on URL, policies, etc. Critical input for SiteInstance decisions.
    *   `content/browser/site_instance_group.cc`/`.h`: Groups SiteInstances sharing a process/AgentSchedulingGroup.
    *   `content/public/browser/site_instance.h` (Public API)

## 2. Key Concepts & Interactions
*   **BrowsingInstance:** Owns a set of `SiteInstance`s. `BrowsingInstance::GetSiteInstanceForURL` is the central function that, given a URL, finds an appropriate existing `SiteInstance` within the `BrowsingInstance` or creates a new one. Its decision depends heavily on the `SiteInfo` derived from the URL and whether the site can use the `default_site_instance_`.
*   **SiteInfo:** Represents the security principal (the "site") associated with a URL. It includes the site URL (scheme + eTLD+1 or origin for isolated origins), process lock URL, sandboxing flags, isolation status (COOP/COEP/OAC), storage partition config, etc. The accuracy of `SiteInfo::Create` is paramount for correct SiteInstance assignment.
*   **Default SiteInstance:** A special `SiteInstance` within a `BrowsingInstance` that can host multiple sites *not* requiring dedicated processes. `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance` determines eligibility.
*   **Process Assignment:** `SiteInstanceImpl` usually gets associated with a `RenderProcessHost` via `SiteInstanceGroup`. `SiteInstanceImpl::GetOrCreateProcess` handles this, potentially reusing processes based on `RenderProcessHostImpl::MayReuseAndIsSuitable` and `ProcessReusePolicy`.
*   **Process Locking:** `SiteInstanceImpl::LockProcessIfNeeded` ensures that if a `SiteInstance` requires a dedicated process, the associated `RenderProcessHost` is locked to prevent other sites from using it (`RenderProcessHost::SetProcessLock`).

## 3. Potential Logic Flaws & VRP Relevance (Summary - See site_isolation.md for details)
*   **Incorrect Site/Origin Determination (in `SiteInfo::Create`)**: Leads to wrong `SiteInstance` selection by `BrowsingInstance::GetSiteInstanceForURL`.
*   **Incorrect Default SiteInstance Usage (in `CanBePlacedInDefaultSiteInstance`)**: Placing an isolated site into the default process.
*   **Process Allocation/Reuse Errors (in `GetOrCreateProcess`, `MayReuseAndIsSuitable`)**: Sharing processes inappropriately.
*   **Process Lock Bypass/Errors (in `LockProcessIfNeeded`, `SetProcessLock`)**: Allowing wrong content into a locked process.
*   **Navigation/Redirect Flaws**: State confusion affecting `SiteInstance` selection during navigation.
*   **IPC/Mojo Vulnerabilities**: Compromised renderers abusing IPC to influence browser-side logic.

## 4. Functions and Methods (Key Ones for Isolation Logic)
*   **`BrowsingInstance::GetSiteInstanceForURL`**: Central logic for finding/creating `SiteInstance` based on `UrlInfo`. Relies on `SiteInfo::Create` and `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`.
*   **`SiteInfo::Create`**: Determines the site, lock URL, isolation properties, etc., from a `UrlInfo`. Critical for correct classification.
*   **`SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`**: Determines if a site *requires* a dedicated process.
*   **`SiteInstanceImpl::IsSuitableForUrlInfo`**: Checks if *this* `SiteInstance` can host a given `UrlInfo`. Considers SiteInfo match, sandboxing, process suitability (`RenderProcessHostImpl::IsSuitableHost`).
*   **`SiteInstanceImpl::GetOrCreateProcess`**: Finds or creates the `RenderProcessHost` for the `SiteInstance`, considering reuse policies and potentially triggering process creation via `RenderProcessHostImpl::GetProcessHostForSiteInstance`.
*   **`RenderProcessHostImpl::MayReuseAndIsSuitable`**: Checks if an existing process can be reused for a `SiteInstance`.
*   **`SiteInstanceImpl::LockProcessIfNeeded`**: Applies the process lock based on `SiteInfo`.
*   **`SiteInstanceImpl::SetSite` / `SetSiteInfoInternal`**: Assigns a `SiteInfo` to an unassigned `SiteInstance`.

## 5. Areas Requiring Further Investigation (Highlights)
*   The precise logic within `SiteInfo::Create` for handling various URL schemes, sandboxing, COOP/COEP/OAC isolation flags, and StoragePartitions.
*   The conditions checked by `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`.
*   The conditions checked by `RenderProcessHostImpl::MayReuseAndIsSuitable`.
*   How `UrlInfo` is constructed during different navigation scenarios (`NavigationRequest`) and passed to `GetSiteInstanceForURL`.
*   Interaction with effective URLs and hosted apps (`ShouldCompareEffectiveURLs`).

## 6. Related VRP Reports
*(See [site_isolation.md](site_isolation.md) for a comprehensive list)*

*(See also: [site_info.md](site_info.md), [browsing_instance.md](browsing_instance.md), [site_instance_group.md](site_instance_group.md), [site_isolation.md](site_isolation.md), [render_process_host.md](render_process_host.md), [navigation_request.md](navigation_request.md))*
