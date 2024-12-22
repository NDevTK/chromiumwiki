# BrowsingInstance

This page details the `BrowsingInstance` class and its role in site isolation.

## Core Concepts

A `BrowsingInstance` represents a group of `SiteInstance`s that share the same browsing context. It is a key component in managing the relationships between different sites and processes. Each `BrowsingInstance` is associated with a `CoopRelatedGroup`, which determines which pages can communicate with each other via same-origin scripting.

### Key Areas of Concern

-   Incorrectly managing `BrowsingInstance`s can lead to inconsistent isolation decisions across tabs and windows.
-   Errors in determining which `SiteInstance`s belong to the same `BrowsingInstance` can lead to security vulnerabilities.

### Related Files

-   `content/browser/browsing_instance.h`
-   `content/browser/browsing_instance.cc`

### Functions and Methods

-   `BrowsingInstance()`: Constructor for creating a new `BrowsingInstance`. It takes the following parameters:
    -   `context`: The `BrowserContext` to which this `BrowsingInstance` belongs.
    -   `web_exposed_isolation_info`: Indicates whether the `BrowsingInstance` should contain only cross-origin isolated pages.
    -   `is_guest`: Specifies whether this `BrowsingInstance` will be used in a `<webview>` guest.
    -   `is_fenced`: Specifies whether this `BrowsingInstance` is used inside a fenced frame.
    -   `is_fixed_storage_partition`: Indicates whether the current `StoragePartition` will apply to future navigations.
    -   `coop_related_group`: The `CoopRelatedGroup` to which this `BrowsingInstance` belongs.
    -   `common_coop_origin`: If set, indicates that all documents hosted by the `BrowsingInstance` have the same COOP value defined by the given origin.
-   `~BrowsingInstance()`: Destructor for the `BrowsingInstance`. It removes any origin isolation opt-ins associated with this instance.
-   `GetBrowserContext()`: Returns the associated `BrowserContext`.
-   `isolation_context()`: Returns the `IsolationContext` associated with this `BrowsingInstance`.
-   `is_fixed_storage_partition()`: Returns true if the `StoragePartition` should be preserved across future navigations.
-   `site_instance_group_manager()`: Returns the `SiteInstanceGroupManager` that controls the `SiteInstanceGroup`s associated with this `BrowsingInstance`.
-   `HasSiteInstance()`: Checks if a `SiteInstance` exists for a given `SiteInfo`.
-   `GetSiteInstanceForURL()`: Gets or creates a `SiteInstance` for a given URL.
-   `GetSiteInstanceForSiteInfo()`: Gets or creates a `SiteInstance` for a given `SiteInfo`.
-   `GetMaybeGroupRelatedSiteInstanceForURL()`: Gets a `SiteInstance` in the same `SiteInstanceGroup`.
-   `GetCoopRelatedSiteInstanceForURL()`: Gets a `SiteInstance` in the same `CoopRelatedGroup`.
-   `GetSiteInfoForURL()`: Gets the `SiteInfo` for a given URL.
-   `GetSiteInstanceForURLHelper()`: Helper for `GetSiteInstanceForURL` and `GetSiteInfoForURL`.
-   `RegisterSiteInstance()`: Registers a `SiteInstance` with the `BrowsingInstance`.
-   `UnregisterSiteInstance()`: Unregisters a `SiteInstance`.
-   `coop_related_group_token()`: Returns the token uniquely identifying the `CoopRelatedGroup` this `BrowsingInstance` belongs to.
-   `token()`: Returns the unique token for this `BrowsingInstance`.
-   `GetCoopRelatedGroupActiveContentsCount()`: Returns the number of active contents in the `CoopRelatedGroup`.
-   `IncrementActiveContentsCount()`: Increments the active contents count.
-   `DecrementActiveContentsCount()`: Decrements the active contents count.
-   `HasDefaultSiteInstance()`: Checks if a default `SiteInstance` exists.
-   `ComputeSiteInfoForURL()`: Computes the `SiteInfo` for a given URL.
-   `EstimateOriginAgentClusterOverhead()`: Estimates overhead due to origin-keyed processes.
-   `web_exposed_isolation_info()`: Returns the web-exposed isolation status.
-   `default_site_instance()`: Returns the default `SiteInstance`.
-   `common_coop_origin()`: Returns the common COOP origin.
-   `NextBrowsingInstanceId()`: Returns the ID for the next `BrowsingInstance` to be created.

### Member Variables

-   `next_browsing_instance_id_`: Static variable tracking the next available `BrowsingInstance` ID.
-   `isolation_context_`: Stores the `IsolationContext` associated with this `BrowsingInstance`.
-   `site_instance_group_manager_`: Manages `SiteInstanceGroup` objects for this `BrowsingInstance`.
-   `active_contents_count_`: Tracks the number of active contents in this `BrowsingInstance`.
-   `default_site_instance_`: Stores the default `SiteInstance` used for sites that don't require dedicated processes.
-   `web_exposed_isolation_info_`: Stores the web-exposed isolation status of this `BrowsingInstance`.
-   `storage_partition_config_`: Stores the `StoragePartitionConfig` that must be used by all `SiteInstance`s in this `BrowsingInstance`.
-   `coop_related_group_`: Stores a reference to the associated `CoopRelatedGroup`.
-   `common_coop_origin_`: If set, indicates that all documents in this `BrowsingInstance` share the same COOP value defined by the given origin.
-   `is_fixed_storage_partition_`: Indicates whether the `StoragePartition` should be preserved across future navigations.
-   `token_`: Stores the unique token for this `BrowsingInstance`.
-   `site_instance_map_`: A map of `SiteInfo` to `SiteInstanceImpl`, ensuring only one `SiteInstance` per site within the `BrowsingInstance`.

### Further Investigation

-   The logic for determining which `SiteInstance`s belong to the same `BrowsingInstance`.
-   The interaction between `BrowsingInstance`, `SiteInstance`, and `SiteInstanceGroup` in managing processes and enforcing site isolation.
-   The impact of incorrect `BrowsingInstance` management on cross-origin communication and data access.
-   The role of `CoopRelatedGroup` in managing communication between `BrowsingInstance`s.
-   The implications of the `web_exposed_isolation_info_` and `common_coop_origin_` on site isolation and process allocation.
-   The usage of `default_site_instance_` and its impact on security and performance.
-   How `BrowsingInstance` management interacts with features like `<webview>`, fenced frames, and service workers.
-   The relationship between `BrowsingInstance` and the renderer-side `blink::Page::RelatedPages()` method.

### Files Analyzed:

-   `chromiumwiki/README.md`
-   `content/browser/url_info.cc`
-   `chromiumwiki/url_info.md`
-   `content/browser/url_info.h`
-   `content/browser/site_info.cc`
-   `chromiumwiki/site_info.md`
-   `content/browser/site_info.h`
-   `content/browser/site_instance_impl.cc`
-   `chromiumwiki/site_instance.md`
-   `content/browser/site_instance_impl.h`
-   `content/browser/site_instance_group.cc`
-   `chromiumwiki/site_instance_group.md`
-   `content/browser/site_instance_group.h`
-   `content/browser/site_instance_group_manager.cc`
-   `content/browser/browsing_instance.h`
-   `content/browser/browsing_instance.cc`
