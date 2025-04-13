# BrowsingInstance

## 1. Component Focus
*   **Functionality:** Represents a "unit of related browsing contexts" (HTML spec), intuitively a collection of tabs/frames that can script each other. A `BrowsingInstance` contains one or more `SiteInstance`s. It holds state common to all SiteInstances within it, such as the `BrowserContext`, cross-origin isolation status (`web_exposed_isolation_info`), and guest/fenced frame status. It's crucial for maintaining process model consistency *within* a group of related contexts.
*   **Key Logic:** Managing the set of `SiteInstance`s within its scope (`site_instance_map_`, `default_site_instance_`), providing the correct `SiteInstance` for a given URL (`GetSiteInstanceForURL`), managing relationships with `CoopRelatedGroup`.
*   **Core Files:**
    *   `content/browser/browsing_instance.h`
    *   `content/browser/browsing_instance.cc`

## 2. Key Concepts & Interactions
*   **SiteInstance:** The primary building block *within* a `BrowsingInstance`. Each `SiteInstance` represents a specific security principal (site) within the `BrowsingInstance`. See [site_instance.md](site_instance.md).
*   **SiteInfo:** Defines the security characteristics of a site based on a URL and context. Crucial input for deciding which `SiteInstance` to use. See [site_info.md](site_info.md).
*   **UrlInfo:** Packages a URL with additional contextual security information (headers, sandbox status, initiator origin). Input to `SiteInfo::Create`. See [url_info.md](url_info.md).
*   **Default SiteInstance (`default_site_instance_`):** A potentially shared `SiteInstance` within the `BrowsingInstance` used for sites that *do not* require a dedicated process (as determined by `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`).
*   **SiteInstance Map (`site_instance_map_`):** Maps specific `SiteInfo` keys to their dedicated `SiteInstanceImpl*` objects within this `BrowsingInstance`. Ensures only one dedicated `SiteInstance` per site. Does *not* contain the default SiteInstance.
*   **CoopRelatedGroup:** A potentially larger grouping than `BrowsingInstance`. Multiple `BrowsingInstance`s can belong to the same `CoopRelatedGroup` if they are related via COOP: restrict-properties. See [coop_coep.md](coop_coep.md).
*   **IsolationContext:** Bundles the `BrowsingInstanceId`, `BrowserContext`, guest/fenced status, and default OAC state. Passed around to make isolation decisions.

## 3. SiteInstance Selection within a BrowsingInstance (`GetSiteInstanceForURL`)

When code needs a `SiteInstance` *within* the current `BrowsingInstance` (i.e., a BrowsingInstance swap is *not* occurring), it typically calls `BrowsingInstance::GetSiteInstanceForURL`.

**Logic (`GetSiteInstanceForURL` / `GetSiteInstanceForURLHelper`):**

1.  **Compute SiteInfo:** Calls `ComputeSiteInfoForURL` using the input `UrlInfo` and the `BrowsingInstance`'s `IsolationContext`. This determines the canonical `SiteInfo` for the URL *within this BI's context*.
2.  **Check Specific Map:** Looks up the computed `SiteInfo` in `site_instance_map_`.
    *   If found: Returns the existing dedicated `SiteInstanceImpl*`.
3.  **Check Default SiteInstance:** If no specific instance was found:
    *   Calls `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance` using the `IsolationContext`, URL, and computed `SiteInfo` to check if the site requires a dedicated process.
    *   Checks if the caller allowed using the default instance (`allow_default_instance` parameter).
    *   If both conditions are met:
        *   Gets or creates the `default_site_instance_` for this `BrowsingInstance`. If created, it's assigned the default site URL (`"http://unisolated.invalid"`) and registered.
        *   Calls `site_instance->AddSiteInfoToDefault(site_info)` to track which specific sites are using the default instance.
        *   Returns the `default_site_instance_`.
4.  **Create New Specific Instance:** If neither an existing specific instance nor the default instance was returned:
    *   Creates a new `SiteInstanceImpl` belonging to this `BrowsingInstance`.
    *   Optionally assigns the computed `SiteInfo` immediately via `SetSite` based on `SiteInstanceImpl::ShouldAssignSiteForUrlInfo` (e.g., not done initially for `about:blank`). Assigning the site also registers it in `site_instance_map_`.
    *   Optionally adds the new `SiteInstance` to a `creation_group` if provided.
    *   Returns the new `SiteInstanceImpl`.

**Key Points:**

*   This logic operates *within* a single `BrowsingInstance`. It does not handle swaps *between* `BrowsingInstance`s (that's managed by `RenderFrameHostManager`).
*   The decision heavily relies on `SiteInfo::Create` and `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`.
*   The `default_site_instance_` allows process consolidation for non-isolated sites but requires careful tracking (`AddSiteInfoToDefault`).

## 4. Potential Logic Flaws & VRP Relevance
*   **Incorrect Default SiteInstance Usage:** A bug in `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance` could allow a site requiring isolation to be placed in the shared default process.
*   **SiteInfo Mismatch:** If `ComputeSiteInfoForURL` calculates a different `SiteInfo` than expected (e.g., due to different `IsolationContext`), it could lead to returning the wrong `SiteInstance` (either a different specific one or unnecessarily using/not using the default one).
*   **Map Management Errors:** Incorrect registration/unregistration in `site_instance_map_` or incorrect handling of `default_site_instance_` could lead to multiple `SiteInstance` objects for the same site within the BI, breaking assumptions and potentially process isolation.
*   **Race Conditions:** If multiple requests try to get a `SiteInstance` for the same new site concurrently, could it lead to multiple instances being created before registration? (The code appears robust against this by checking the map again after creation, but complex interactions might exist).

## 5. Functions and Methods (Key Ones)
*   **`BrowsingInstance()`**: Constructor. Initializes context, registers with `CoopRelatedGroup`.
*   **`~BrowsingInstance()`**: Destructor. Unregisters from `CoopRelatedGroup`, removes OAC opt-ins.
*   **`GetSiteInstanceForURL()` (Overloads)**: Public entry point to get/create a `SiteInstance` within this BI. Calls `GetSiteInstanceForURLHelper` and potentially creates a new instance.
*   **`GetSiteInstanceForURLHelper()`**: Private helper containing core logic to check `site_instance_map_` and potentially use/create `default_site_instance_`.
*   **`GetSiteInstanceForSiteInfo()`**: Gets/creates a *specific* `SiteInstance` for a given `SiteInfo`, bypassing the default instance logic.
*   **`GetCoopRelatedSiteInstanceForURL()`**: Delegates to `CoopRelatedGroup` to find/create an SI, potentially in a *different* BI within the same group.
*   **`RegisterSiteInstance()` / `UnregisterSiteInstance()`**: Manage entries in `site_instance_map_` and `default_site_instance_`.
*   **`ComputeSiteInfoForURL()`**: Helper to create a `SiteInfo` using this BI's `IsolationContext`. Ensures consistent `StoragePartitionConfig`.

## 6. Areas Requiring Further Investigation
*   The detailed logic of `SiteInstanceImpl::CanBePlacedInDefaultSiteInstance` and `SiteInstanceImpl::ShouldAssignSiteForUrlInfo`.
*   How the `default_site_instance_` interacts with cross-origin isolation (`web_exposed_isolation_info_`). (Comments suggest it can hold isolated instances under memory pressure, which seems potentially risky).
*   Potential race conditions around `SiteInstance` creation and registration, especially involving `default_site_instance_`.
*   How `GetCoopRelatedSiteInstanceForURL` selects a `BrowsingInstance` within the `CoopRelatedGroup`.

## 7. Related VRP Reports
*   *(Likely related to VRPs involving incorrect process placement if the root cause involves faulty logic within a BrowsingInstance, particularly misuse of the default SiteInstance.)*

*(See also: [site_instance.md](site_instance.md), [site_info.md](site_info.md), [coop_coep.md](coop_coep.md), [site_isolation.md](site_isolation.md))*
