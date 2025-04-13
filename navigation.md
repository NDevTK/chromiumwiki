# Navigation Process Model

## 1. Component Focus
*   **Functionality:** Governs how Chromium determines the appropriate `SiteInstance`, `BrowsingInstance`, and `RenderProcessHost` for a navigation, ensuring security boundaries (site isolation) and performance optimizations (process reuse, BackForwardCache) are correctly applied.
*   **Key Logic:** Determining if a BrowsingInstance swap is needed (`ShouldSwapBrowsingInstancesForNavigation`), selecting or creating the target `SiteInstance` (`GetSiteInstanceForNavigation`, `DetermineSiteInstanceForURL`), potentially reusing processes (`GetProcessHostForSiteInstance`), handling redirects, and managing speculative RenderFrameHosts.
*   **Core Files:**
    *   `content/browser/renderer_host/render_frame_host_manager.cc`: Manages RenderFrameHosts within a FrameTreeNode, central orchestrator for SiteInstance selection.
    *   `content/browser/renderer_host/navigation_request.cc`: Represents a single navigation, holds state used for decisions.
    *   `content/browser/site_instance_impl.*`, `content/browser/browsing_instance.*`, `content/browser/site_info.*`, `content/browser/process_lock.*`, `content/browser/renderer_host/render_process_host_impl.*`, `content/browser/child_process_security_policy_impl.*`: Provide underlying data structures and policy checks.

## 2. SiteInstance Selection Overview

The core logic resides in `RenderFrameHostManager::GetSiteInstanceForNavigation`. It performs several steps:

1.  **Check for BrowsingInstance Swap:** Calls `ShouldSwapBrowsingInstancesForNavigation` (see Section 3).
2.  **Determine Target SiteInstance Type:** Calls `DetermineSiteInstanceForURL` based on swap decision, source/destination/current SiteInstances, transition type, etc., returning a `SiteInstanceDescriptor` indicating the desired relationship (e.g., `RELATED`, `UNRELATED`).
3.  **Resolve SiteInstance:** Calls `ConvertToSiteInstance` to get/create the actual `SiteInstanceImpl` based on the descriptor.
4.  **Apply Policies:** Sets process reuse policies on the chosen `SiteInstance`.

## 3. BrowsingInstance Swapping (`ShouldSwapBrowsingInstancesForNavigation`)

This function determines if a navigation *must* or *should* leave the current `BrowsingInstance` and start a new one. A new `BrowsingInstance` generally implies a new `SiteInstance` and potentially a new process, breaking script connections to the old `BrowsingInstance`.

**Conditions checked (roughly in order):**

*   **No Swap Reasons:**
    *   Subframe navigation (always stays with parent).
    *   Same-document navigation.
    *   Renderer debug URL (e.g., `chrome://crash`).
    *   History navigation to a related SiteInstance (checked later).
    *   Proactive swap disabled or not applicable (checks for BFCache eligibility, existing related contents, `rel=opener`, etc.). See `ShouldProactivelySwapBrowsingInstance`.
*   **Mandatory Security/Policy Swaps:**
    *   COOP header requirement (`CoopSwapResult::kSwap`).
    *   Cross-`BrowserContext` navigation.
    *   WebUI security transitions (WebUI <-> non-WebUI, or different WebUI types), unless starting from an initial blank frame.
    *   Embedder policy (`ContentBrowserClient::ShouldSwapBrowsingInstancesForNavigation`).
    *   View-source mode transitions.
    *   Navigating to view-source from an uninitialized SiteInstance.
    *   Dynamic Origin Isolation policy (if navigating to a newly isolated origin and swap is safe).
    *   Storage Partition change.
    *   Prefetch potentially contaminated by cross-site state (feature flag controlled).
    *   History navigation to an unrelated SiteInstance.
    *   Cross-site browser-initiated navigation heuristic (to avoid unwanted process sharing).
*   **Conditional Swaps:**
    *   COOP Related Swap (`CoopSwapResult::kSwapRelated`): Swaps BI but keeps them related via `CoopRelatedGroup`.
    *   Proactive Swap for BackForwardCache (`ShouldProactivelySwapBrowsingInstance`): Swaps BI if BFCache is enabled and the current page seems eligible, aiming to improve cacheability.

The function returns a `BrowsingContextGroupSwap` object indicating the swap type (`kNoSwap`, `kSecuritySwap`, `kCoopSwap`, `kRelatedCoopSwap`, `kProactiveSwap`) and the specific reason (`ShouldSwapBrowsingInstance` enum).

## 4. SiteInstance Type Determination (`DetermineSiteInstanceForURL`)

Based on the swap decision and other factors, this determines the *kind* of SiteInstance needed, returning a `SiteInstanceDescriptor`.

*   If an error page needs the current process, uses `current_instance`.
*   If an error page needs an isolated process, creates a descriptor for an error SiteInstance (in a related or unrelated BI based on the original swap decision).
*   If navigating back/forward to a suitable `dest_instance`, uses that.
*   If COOP requires a related swap, returns a `RELATED_IN_COOP_GROUP` descriptor.
*   If a mandatory security swap is required, returns an `UNRELATED` descriptor (unless `source_instance` can be reused for specific `about:`/`data:` URLs in a different BI).
*   Checks if `source_instance` should be used (`CanUseSourceSiteInstance` for `about:`/`data:` URLs).
*   Handles NTP subframe special cases.
*   If `current_instance` has no site yet and meets criteria, uses it.
*   If same-site, uses `current_instance`.
*   Checks for reusing parent's or opener's SiteInstance for subframes based on site comparison (`IsCandidateSameSite`).
*   Handles consolidation for non-strictly-isolated subframes (e.g., `file:` URLs).
*   Defaults to returning a `RELATED` or `UNRELATED` descriptor based on COI compatibility.

## 5. SiteInstance Resolution (`ConvertToSiteInstance`)

Takes the `SiteInstanceDescriptor` and returns the actual `SiteInstanceImpl`:

*   If descriptor points to an `existing_site_instance`, returns it.
*   If `RELATED_IN_GROUP`, calls `GetMaybeGroupRelatedSiteInstanceImpl` on `source_instance`.
*   If `RELATED`, calls `GetRelatedSiteInstanceImpl` on `current_instance`.
*   If `RELATED_IN_COOP_GROUP`, calls `GetCoopRelatedSiteInstanceImpl` on `current_instance`.
*   If `UNRELATED`:
    *   Checks if `candidate_instance` (e.g., from redirects) matches the descriptor.
    *   Otherwise, creates a brand new SiteInstance using `SiteInstanceImpl::CreateForUrlInfo`.

## 6. Potential Logic Flaws & VRP Relevance
*   **Incorrect Swap Decision (`ShouldSwapBrowsingInstancesForNavigation`)**: Failing to swap when required (e.g., for WebUI, COOP, isolated origins) or swapping unnecessarily (breaking script connections, hurting performance).
*   **Incorrect SiteInstance Type (`DetermineSiteInstanceForURL`)**: Choosing the wrong relationship (e.g., `RELATED` vs. `UNRELATED`) can lead to incorrect BI assignment.
*   **State Consistency**: Bugs related to using stale or incorrect state (e.g., `current_effective_url`, `is_same_site` caching) in the swap decision.
*   **Policy Enforcement Gaps**: Missing checks for specific policies (COOP, dynamic isolation, embedder policies).
*   **Proactive Swap Issues**: Incorrectly deciding to proactively swap (breaking legitimate scripting) or failing to swap when beneficial for BFCache.

*(See also: [site_instance.md](site_instance.md), [browsing_instance.md](browsing_instance.md), [site_info.md](site_info.md), [url_info.md](url_info.md), [site_isolation.md](site_isolation.md), [coop_coep.md](coop_coep.md))*