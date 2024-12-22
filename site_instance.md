# SiteInstance

This page details the `SiteInstanceImpl` class and its role in site isolation.

## Core Concepts

The `SiteInstanceImpl` class represents an instance of a site and is responsible for managing the associated process. It plays a crucial role in enforcing site isolation.

### Key Areas of Concern

-   Incorrectly assigning a process to a `SiteInstanceImpl` can lead to different sites sharing the same process, breaking site isolation.
-   Errors in determining whether a process can be reused for a given `SiteInstanceImpl` can lead to process reuse vulnerabilities.
-   Incorrectly handling the site URL can lead to the wrong process being used for a navigation.

### Related Files

-   `content/browser/site_instance_impl.cc`
-   `content/browser/site_instance_impl.h`

### Functions and Methods

-   `SiteInstanceImpl::Create`: Creates a new `SiteInstanceImpl`.
-   `SiteInstanceImpl::CreateForURL`: Creates a new `SiteInstanceImpl` for a given URL.
    -   This method creates a new `SiteInstanceImpl` and BrowsingInstance, and then calls `GetSiteInstanceForURL` on the BrowsingInstance to get the SiteInstance.
-   `SiteInstanceImpl::CreateForGuest`: Creates a new `SiteInstanceImpl` for a guest view.
-   `SiteInstanceImpl::CreateForFixedStoragePartition`: Creates a new `SiteInstanceImpl` for a fixed storage partition.
-   `SiteInstanceImpl::CreateForFencedFrame`: Creates a new `SiteInstanceImpl` for a fenced frame.
    -   This method reuses the embedder's SiteInfo and process if possible.
-   `SiteInstanceImpl::CreateForUrlInfo`: Creates a new `SiteInstanceImpl` for a given `UrlInfo`.
-   `SiteInstanceImpl::CreateForServiceWorker`: Creates a new `SiteInstanceImpl` for a service worker.
    -   This method does not allow the default site instance and attempts to reuse a renderer process if possible.
-   `SiteInstanceImpl::CreateReusableInstanceForTesting`: Creates a reusable `SiteInstanceImpl` for testing purposes.
-   `SiteInstanceImpl::GetProcess`: Manages the process associated with a site, creating a new `SiteInstanceGroup` and `RenderProcessHost` if one doesn't exist.
-   `SiteInstanceImpl::SetSite`: Updates the `SiteInfo` associated with the instance.
    -   This method converts the given URL into a `SiteInfo` and then calls `SetSiteInfoInternal`.
-   `SiteInstanceImpl::SetSiteInfoToDefault`: Sets the `SiteInfo` to the default value.
    -   This method creates a default `SiteInfo` and sets it using `SetSiteInfoInternal`.
-   `SiteInstanceImpl::SetSiteInfoInternal`: Sets the `SiteInfo` for the `SiteInstanceImpl`.
    -   This method registers the `SiteInstanceImpl` with the `BrowsingInstance` and notifies the embedder if the SiteInstance now has both a process and a site assigned.
-   `SiteInstanceImpl::ReuseExistingProcessIfPossible`: Reuses an existing process if it's suitable for the `SiteInstance`.
    -   This method checks if the existing process is suitable for the `SiteInstance`, and if it's not, it will not reuse the process.
    -   It also checks if the destination uses process-per-site, and if it does, it will not reuse the process.
-   `SiteInstanceImpl::SetProcessInternal`: Sets the process for the `SiteInstance`.
    -   This method creates a new `SiteInstanceGroup` if one doesn't exist.
    -   It also calls `LockProcessIfNeeded` to lock the process to the site if necessary.
    -   It also registers the process for the current site if process-per-site is enabled.
-   `SiteInstanceImpl::LockProcessIfNeeded`: Locks the process to the site if necessary.
    -   This method checks if the process is already locked to a site, and if it is, it will not reassign a different origin lock to the process.
    -   It also updates the process lock state to signal that the process has been associated with a `SiteInstance` that is not locked to a site yet.
-   `SiteInstanceImpl::IsForGuestsOnly`: Checks if the `SiteInstance` is for guests only.
-   `SiteInstanceImpl::IsSandboxed`: Checks if the `SiteInstance` is sandboxed.
-   `SiteInstanceImpl::IsForServiceWorker`: Checks if the `SiteInstance` is for a service worker.
-  `SiteInstanceImpl::IsSameSiteWithURL`: Checks if a URL is same-site with the `SiteInstance`.
-   `SiteInstanceImpl::IsNavigationSameSite`: Checks if a navigation is same-site with the `SiteInstance`.
-   `SiteInstanceImpl::GetSiteInfo`: Returns the `SiteInfo` associated with the `SiteInstance`.
-   `SiteInstanceImpl::GetStoragePartitionConfig`: Returns the `StoragePartitionConfig` associated with the `SiteInstance`.
-   `SiteInstanceImpl::ShouldAssignSiteForUrlInfo`: Determines if a site should be assigned for a given `UrlInfo`.
-   `SiteInstanceImpl::GetId`: Returns the ID of the `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetBrowsingInstanceId`: Returns the ID of the `BrowsingInstance` associated with the `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetIsolationContext`: Returns the `IsolationContext` associated with the `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetSiteInstanceGroupProcessIfAvailable`: Returns the process of the `SiteInstanceGroup` if available.
-   `SiteInstanceImpl::IsDefaultSiteInstance`: Returns true if the `SiteInstanceImpl` is the default SiteInstance.
-   `SiteInstanceImpl::AddSiteInfoToDefault`: Adds a `SiteInfo` to the default SiteInstance.
-   `SiteInstanceImpl::IsSiteInDefaultSiteInstance`: Checks if a site is in the default SiteInstance.
-   `SiteInstanceImpl::NextBrowsingInstanceId`: Returns the next BrowsingInstanceId.
-   `SiteInstanceImpl::HasProcess`: Returns true if the `SiteInstanceImpl` has a process.
-   `SiteInstanceImpl::ShouldUseProcessPerSite`: Returns true if the process-per-site model should be used.
-   `SiteInstanceImpl::PreventAssociationWithSpareProcess`: Prevents the `SiteInstanceImpl` from associating with a spare process.
-   `SiteInstanceImpl::CanAssociateWithSpareProcess`: Returns true if the `SiteInstanceImpl` can associate with a spare process.
-   `SiteInstanceImpl::ConvertToDefaultOrSetSite`: Converts the `SiteInstanceImpl` to the default SiteInstance or sets the site.
-   `SiteInstanceImpl::GetLastProcessAssignmentOutcome`: Returns the last process assignment outcome.
-   `SiteInstanceImpl::DeriveSiteInfo`: Derives a `SiteInfo` for a given `UrlInfo`.
-   `SiteInstanceImpl::HasSite`: Returns true if the `SiteInstanceImpl` has a site.
-   `SiteInstanceImpl::HasRelatedSiteInstance`: Returns true if the `SiteInstanceImpl` has a related SiteInstance.
-   `SiteInstanceImpl::GetRelatedSiteInstance`: Returns a related `SiteInstance` for a given URL.
-   `SiteInstanceImpl::GetRelatedSiteInstanceImpl`: Returns a related `SiteInstanceImpl` for a given `UrlInfo`.
-   `SiteInstanceImpl::GetMaybeGroupRelatedSiteInstanceImpl`: Returns a related `SiteInstanceImpl` for a given `UrlInfo` in the same group.
-   `SiteInstanceImpl::GetCoopRelatedSiteInstanceImpl`: Returns a related `SiteInstanceImpl` for a given `UrlInfo` that is COOP related.
-   `SiteInstanceImpl::GetOrCreateAgentSchedulingGroup`: Returns the `AgentSchedulingGroupHost` associated with the `SiteInstanceImpl`.
-   `SiteInstanceImpl::SetSiteInstanceGroup`: Sets the `SiteInstanceGroup` for the `SiteInstanceImpl`.
-   `SiteInstanceImpl::ResetSiteInstanceGroup`: Resets the `SiteInstanceGroup` for the `SiteInstanceImpl`.
-   `SiteInstanceImpl::IsRelatedSiteInstance`: Returns true if the given `SiteInstance` is related to this `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetRelatedActiveContentsCount`: Returns the number of active contents related to this `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetBrowserContext`: Returns the `BrowserContext` associated with the `SiteInstanceImpl`.
-   `SiteInstanceImpl::SetProcessForTesting`: Sets the process for the `SiteInstanceImpl` for testing purposes.
-   `SiteInstanceImpl::IncrementActiveDocumentCount`: Increments the active document count for a given `SiteInfo`.
-   `SiteInstanceImpl::DecrementActiveDocumentCount`: Decrements the active document count for a given `SiteInfo`.
-   `SiteInstanceImpl::GetActiveDocumentCount`: Returns the active document count for a given `SiteInfo`.
-   `SiteInstanceImpl::IsCrossOriginIsolated`: Returns true if the `SiteInstanceImpl` is cross-origin isolated.
-   `SiteInstanceImpl::GetCommonCoopOrigin`: Returns the common COOP origin of the `SiteInstanceImpl`.
-   `SiteInstanceImpl::IsNavigationAllowedToStayInSameProcessDueToEffectiveURLs`: Returns true if a navigation to a given URL should be allowed to stay in the current process due to effective URLs being involved in the navigation, even if the navigation would normally result in a new process.
-   `SiteInstanceImpl::GetPartitionDomain`: Returns the storage partition domain for this object.
-   `SiteInstanceImpl::IsJitDisabled`: Returns true if this `SiteInstanceImpl` is for a site that has JIT disabled.
-   `SiteInstanceImpl::AreV8OptimizationsDisabled`: Returns true if this `SiteInstanceImpl` is for a site that has V8 optimizations disabled.
-   `SiteInstanceImpl::IsPdf`: Returns true if this `SiteInstanceImpl` is for a site that contains PDF contents.
-   `SiteInstanceImpl::group()`: Returns the `SiteInstanceGroup` this `SiteInstanceImpl` belongs to.
-   `SiteInstanceImpl::IsSameSiteWithURLInfo`: Checks if a `UrlInfo` is same-site with the `SiteInstanceImpl`.
-   `SiteInstanceImpl::GetCompatibleSandboxedSiteInstance`: Finds an existing `SiteInstanceImpl` in this `SiteInstanceImpl`'s `BrowsingInstance` that matches the given `UrlInfo` but with the `is_sandboxed_` flag true.
-   `SiteInstanceImpl::GetDefaultProcessForBrowsingInstance`: Returns the process used by non-isolated sites in this `SiteInstanceImpl`'s `BrowsingInstance`.
-   `SiteInstanceImpl::set_process_assignment`: Sets the process assignment outcome for this `SiteInstanceImpl`.
-   `SiteInstanceImpl::set_process_reuse_policy`: Sets the process reuse policy for this `SiteInstanceImpl`.
-   `SiteInstanceImpl::process_reuse_policy`: Returns the process reuse policy for this `SiteInstanceImpl`.
-   `SiteInstanceImpl::is_for_service_worker`: Returns true if this `SiteInstanceImpl` was created for a service worker.
-   `SiteInstanceImpl::original_url`: Returns the URL which was used to set the `site_info_` for this `SiteInstanceImpl`.
-   `SiteInstanceImpl::browsing_instance_token`: Returns the token uniquely identifying the `BrowsingInstance` this `SiteInstanceImpl` belongs to.
-   `SiteInstanceImpl::coop_related_group_token`: Returns the token uniquely identifying the `CoopRelatedGroup` this `SiteInstanceImpl` belongs to.
-   `SiteInstanceImpl::IsOriginalUrlSameSite`: Returns true if the `original_url()` is the same site as the given `UrlInfo` or this object is a default `SiteInstanceImpl` and can be considered the same site as the given `UrlInfo`.
-   `SiteInstanceImpl::AddSiteInfoToDefault`: Adds a `SiteInfo` to the set that tracks what sites have been allowed to be handled by this default `SiteInstanceImpl`.
-   `SiteInstanceImpl::DoesSiteInfoForURLMatch`: Returns true if the `SiteInfo` for the given `UrlInfo` matches the `SiteInfo` for this instance.
-   `SiteInstanceImpl::RegisterAsDefaultOriginIsolation`: Adds an origin as having the default isolation state within this `BrowsingInstance` due to an existing instance at the time of opt-in, so that future instances of it here won't be origin isolated.
-   `SiteInstanceImpl::GetWebExposedIsolationInfo`: Returns the web-exposed isolation status of the `BrowsingInstance` this `SiteInstanceImpl` is part of.
-   `SiteInstanceImpl::IsCoopRelatedSiteInstance`: Returns whether the two `SiteInstanceImpl`s belong to the same `CoopRelatedGroup`.
-   `SiteInstanceImpl::set_destruction_callback_for_testing`: Sets a callback to be run from this `SiteInstanceImpl`'s destructor (used only in tests).

### Member Variables

-   `site_instance_group_`: Stores a reference to the associated `SiteInstanceGroup`.
-   `id_`: Stores the unique ID of the `SiteInstanceImpl`.
-   `browsing_instance_`: Stores a reference to the associated `BrowsingInstance`.
-   `can_associate_with_spare_process_`: Indicates if the `SiteInstanceImpl` can associate with a spare process.
-   `site_info_`: Stores the `SiteInfo` associated with the `SiteInstanceImpl`.
-   `has_site_`: Indicates if the `SiteInstanceImpl` has a site.
-   `process_reuse_policy_`: Stores the process reuse policy.
-   `is_for_service_worker_`: Indicates if the `SiteInstanceImpl` is for a service worker.
-   `process_assignment_`: Stores the last process assignment outcome.
-   `original_url_`: Stores the original URL of the `SiteInstanceImpl`.
-   `default_site_instance_state_`: Stores the state of the default SiteInstance.
-   `verify_storage_partition_info_`: Indicates if the storage partition info should be verified.
-   `active_document_counts_`: Stores the active document counts for different `SiteInfo` objects.

### Further Investigation

-   The logic for determining when a process can be reused for a given `SiteInstanceImpl`.
-   The interaction between `SiteInstanceImpl` and different types of URLs, especially those with unique origins (e.g., blob URLs, filesystem URLs).
-   The impact of incorrect site URL handling on cross-origin communication and data access.
-   The logic within `SiteInstanceImpl::SetProcessInternal` to ensure that the process is correctly locked to the site.
-   The logic within `SiteInstanceImpl::ReuseExistingProcessIfPossible` to ensure that processes are reused correctly.
-   The logic within `SiteInstanceImpl::IsSuitableForUrlInfo` to ensure that the SiteInstance is suitable for a given URL.
-   The logic within `SiteInstanceImpl::IsNavigationSameSite` to ensure that navigations are correctly classified as same-site or cross-site.
-   The usage of `SiteInstanceImpl::SetSiteInfoInternal` to ensure that all fields are correctly initialized.
-   The usage of `SiteInstanceImpl::ConvertToDefaultOrSetSite` to ensure that SiteInstances are correctly converted to the default SiteInstance or have their site set.
-   The logic within `SiteInstanceImpl::LockProcessIfNeeded` to ensure that the process is correctly locked to the site.
-   The logic within `SiteInstanceImpl::DeriveSiteInfo` and how it interacts with different types of URLs and isolation requirements.
-   The interaction between `SiteInstanceImpl`, `SiteInstanceGroup`, and `BrowsingInstance` in managing processes and enforcing site isolation.
-   The handling of edge cases, such as when a `SiteInstanceImpl` is created for a service worker, a guest view, or a fenced frame.
-   The implications of the different process reuse policies and how they affect security and performance.
-   The usage of `active_document_counts_` and how it relates to the management of default `SiteInstanceImpl` objects.

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
