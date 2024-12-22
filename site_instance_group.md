# SiteInstanceGroup

This page details the `SiteInstanceGroup` class and its role in site isolation.

## Core Concepts

A `SiteInstanceGroup` represents a group of `SiteInstance` objects that share the same `RenderProcessHost`. It is used to manage the lifecycle of the `RenderProcessHost` and to ensure that all `SiteInstance` objects in the group are properly isolated. `SiteInstanceGroup` objects are managed by the `SiteInstanceGroupManager`.

### Key Areas of Concern

-   Incorrectly managing the lifecycle of the `RenderProcessHost`.
-   Incorrectly associating `SiteInstance` objects with the `SiteInstanceGroup`.
-   Potential issues with cross-group communication.

### Related Files

-   `content/browser/site_instance_group.h`
-   `content/browser/site_instance_group_manager.cc`
-   `content/browser/site_instance_group.cc`

### Functions and Methods

-   `SiteInstanceGroup::process()`: Returns a raw pointer to the `RenderProcessHost` associated with the group.
-   `SiteInstanceGroup::AddSiteInstance`: Adds a `SiteInstanceImpl` to the group.
-   `SiteInstanceGroup::RemoveSiteInstance`: Removes a `SiteInstanceImpl` from the group.
    -   If the group becomes empty, the associated process is cleaned up.
-   `SiteInstanceGroup::IncrementActiveFrameCount`: Increases the number of active frames in the group.
-   `SiteInstanceGroup::DecrementActiveFrameCount`: Decreases the number of active frames in the group.
    -   If the active frame count reaches zero, observers are notified.
-   `SiteInstanceGroup::IncrementKeepAliveCount`: Increases the number of NavigationStateKeepAlives in the group.
-   `SiteInstanceGroup::DecrementKeepAliveCount`: Decreases the number of NavigationStateKeepAlives in the group.
    -   If the keep alive count reaches zero, observers are notified.
-   `SiteInstanceGroup::IsRelatedSiteInstanceGroup`: Checks if another `SiteInstanceGroup` is in the same `BrowsingInstance`.
-   `SiteInstanceGroup::IsCoopRelatedSiteInstanceGroup`: Checks if another `SiteInstanceGroup` is in the same `CoopRelatedGroup`.
-   `SiteInstanceGroup::CreateForTesting`: Creates a new `SiteInstanceGroup` for testing purposes.
-   `SiteInstanceGroup::RenderProcessHostDestroyed`: Handles the destruction of the associated `RenderProcessHost`.
    -   This method removes references to `this` from all `SiteInstanceImpl` objects in the group.
-   `SiteInstanceGroup::RenderProcessExited`: Handles the exit of the associated `RenderProcessHost`.
    -   This method notifies observers of the process exit.
-   `SiteInstanceGroup::GetStoragePartitionConfig`: Returns the `StoragePartitionConfig` associated with the group.
-   `SiteInstanceGroup::GetId`: Returns the ID of the `SiteInstanceGroup`.
-   `SiteInstanceGroup::GetSafeRef`: Returns a `base::SafeRef` to the `SiteInstanceGroup`.
-   `SiteInstanceGroup::GetWeakPtrToAllowDangling`: Returns a `base::WeakPtr` to the `SiteInstanceGroup` that allows dangling.
-   `SiteInstanceGroup::AddObserver`: Adds an observer to the `SiteInstanceGroup`.
-   `SiteInstanceGroup::RemoveObserver`: Removes an observer from the `SiteInstanceGroup`.
-   `SiteInstanceGroup::WriteIntoTrace`: Writes the `SiteInstanceGroup` data into a trace proto for debugging and analysis.
-   `SiteInstanceGroup::active_frame_count()`: Returns the number of active frames in this `SiteInstanceGroup`.
-   `SiteInstanceGroup::keep_alive_count()`: Returns the number of `NavigationStateKeepAlives` in this `SiteInstanceGroup`.
-   `SiteInstanceGroup::browsing_instance_id()`: Returns the `BrowsingInstanceId` of the `BrowsingInstance` this `SiteInstanceGroup` belongs to.
-   `SiteInstanceGroup::browsing_instance_token()`: Returns the token uniquely identifying the `BrowsingInstance` this `SiteInstanceGroup` belongs to.
-   `SiteInstanceGroup::coop_related_group_token()`: Returns the token uniquely identifying the `CoopRelatedGroup` this `SiteInstanceGroup` belongs to.
-   `SiteInstanceGroup::agent_scheduling_group()`: Returns the `AgentSchedulingGroupHost` associated with this `SiteInstanceGroup`.
-   `SiteInstanceGroup::site_instances_for_testing()`: Returns the set of `SiteInstanceImpl` objects that belong to this group (for testing purposes).

### Member Variables

-   `process_`: Stores a `base::SafeRef` to the `RenderProcessHost` associated with the group.
-   `agent_scheduling_group_`: Stores a `base::SafeRef` to the `AgentSchedulingGroupHost` associated with the group.
-   `site_instances_`: Stores a set of `SiteInstanceImpl` objects that belong to this group.
-   `id_`: Stores the unique ID of the `SiteInstanceGroup`.
-   `browsing_instance_`: Stores a raw pointer to the associated `BrowsingInstance`.
-   `observers_`: Stores a list of observers for the `SiteInstanceGroup`.
-   `active_frame_count_`: Stores the number of active frames in the group.
-   `keep_alive_count_`: Stores the number of NavigationStateKeepAlives in the group.
-   `is_notifying_observers_`: Indicates if the group is currently notifying observers.
-   `weak_ptr_factory_`: Stores a `base::WeakPtrFactory` for the `SiteInstanceGroup`.

### SiteInstanceGroupManager

The `SiteInstanceGroupManager` class is responsible for managing `SiteInstanceGroup` objects and their association with `RenderProcessHost`s. It provides the following methods:

-   `SiteInstanceGroupManager::GetExistingGroupProcess`: Returns an existing process for a given `SiteInstanceImpl` if one is available and suitable.
-   `SiteInstanceGroupManager::OnSiteInfoSet`: Called when a `SiteInstanceImpl`'s `SiteInfo` is set. It may set a default process for the `SiteInstanceGroupManager` if the `kProcessSharingWithStrictSiteInstances` feature is enabled and the `SiteInstanceImpl` has a process but doesn't require a dedicated one.
-   `SiteInstanceGroupManager::OnProcessSet`: Called when a process is set for a `SiteInstanceImpl`. It may set a default process for the `SiteInstanceGroupManager` if one hasn't been set yet and the `kProcessSharingWithStrictSiteInstances` feature is enabled.
-   `SiteInstanceGroupManager::MaybeSetDefaultProcess`: Sets the default process for the `SiteInstanceGroupManager` if the `kProcessSharingWithStrictSiteInstances` feature is enabled and the given `SiteInstanceImpl` has a process, has a site, and doesn't require a dedicated process.
-   `SiteInstanceGroupManager::RenderProcessHostDestroyed`: Called when a `RenderProcessHost` is destroyed. It clears the default process if it matches the destroyed host.
-   `SiteInstanceGroupManager::ClearDefaultProcess`: Clears the default process and removes the `SiteInstanceGroupManager` as an observer.

### Further Investigation

-   The logic for managing the lifecycle of the `RenderProcessHost`.
-   The interaction between `SiteInstanceGroup` and `SiteInstance` objects.
-   The impact of incorrect `SiteInstanceGroup` management on cross-origin communication and data access.
-   The logic within `SiteInstanceGroup::RenderProcessHostDestroyed` to ensure that all references to the `SiteInstanceGroup` are removed when the process is destroyed.
-   The logic within `SiteInstanceGroup::RenderProcessExited` to ensure that observers are notified when the process exits.
-   The usage of `base::SafeRef` and `base::WeakPtr` to manage the lifetime of the `SiteInstanceGroup` and its associated objects.
-   The interaction between `SiteInstanceGroupManager` and `SiteInstanceGroup` in managing the default process and enforcing process reuse policies.
-   The implications of the `kProcessSharingWithStrictSiteInstances` feature on process allocation and site isolation.
-   The role of `active_frame_count_` and `keep_alive_count_` in managing the lifecycle of `SiteInstanceGroup` objects and their associated processes.

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
