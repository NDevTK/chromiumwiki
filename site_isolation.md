# Site Isolation & Process Model

## 1. Component Focus
*   **Functionality:** Encompasses the core mechanisms Chromium uses to separate websites into different processes based on their "site" (typically scheme + eTLD+1) to mitigate security risks like Spectre and improve robustness. This includes determining site boundaries, assigning sites to processes, managing process locks, handling exceptions (e.g., isolated origins, agent clusters, WebUI), and enforcing policies related to process allocation during navigation.
*   **Key Logic:** Site definition (`SiteInfo`), SiteInstance assignment (`BrowsingInstance`, `SiteInstanceImpl`, `RenderFrameHostManager`), process allocation (`RenderProcessHostImpl`), process locking (`ProcessLock`), handling of isolated origins and agent clusters (OAC), COOP/COEP enforcement influencing process decisions.
*   **Core Files:**
    *   `content/browser/site_info.*`
    *   `content/browser/site_instance_impl.*`
    *   `content/browser/browsing_instance.*`
    *   `content/browser/site_instance_group.*`
    *   `content/browser/renderer_host/render_frame_host_manager.*`
    *   `content/browser/renderer_host/render_process_host_impl.*`
    *   `content/browser/child_process_security_policy_impl.*`
    *   `content/public/browser/site_isolation_policy.*`
    *   `content/browser/renderer_host/navigation_request.cc` (Determines target SiteInstance)
    *   `content/browser/web_contents/web_contents_impl.cc` (Handles popup creation)

## 2. Key Concepts
*   **Site:** A security principal, typically scheme + eTLD+1 (e.g., `https://google.com`), but can be finer-grained (origin-level) for isolated origins or OAC. Defined by `SiteInfo`. See [site_info.md](site_info.md).
*   **SiteInstance:** Represents a specific site within a `BrowsingInstance`. Typically maps 1:1 to a `SiteInstanceGroup`. Key object for tracking the site associated with a frame's content. See [site_instance.md](site_instance.md).
*   **BrowsingInstance:** A collection of frames/tabs that can potentially script each other (share a JavaScript execution context). Defines the broadest boundary for synchronous scripting access. See [browsing_instance.md](browsing_instance.md).
*   **SiteInstanceGroup:** A collection of `SiteInstance`s that share a `RenderProcessHost` and `AgentSchedulingGroupHost`. Frames within the same group run in the same process. See [site_instance_group.md](site_instance_group.md).
*   **Process Lock:** A security property associated with a `RenderProcessHost`, restricting which sites/origins it can host based on the first site committed. See [process_lock.md](process_lock.md).
*   **Isolated Origins:** Origins explicitly configured (via policy or command-line) to always require a dedicated process, potentially isolating them more strictly than just by site.
*   **Origin-Agent-Cluster (OAC):** A header (`Origin-Agent-Cluster: ?1`) allowing origins to request logical and potentially process isolation based on origin rather than site.
*   **COOP/COEP:** Headers (`Cross-Origin-Opener-Policy`, `Cross-Origin-Embedder-Policy`) enabling cross-origin isolation, unlocking features like `SharedArrayBuffer`. Can influence process model decisions (e.g., requiring BrowsingInstance swaps). See [coop_coep.md](coop_coep.md).

## 3. Core Isolation Logic
*   **Site Definition (`SiteInfo`, `GetSiteForURLInternal`):** Determining the "site" boundary for a given URL. Errors here can lead to incorrect process placement.
*   **Dedicated Process Requirement (`SiteInfo::RequiresDedicatedProcess`, `ChildProcessSecurityPolicyImpl::IsIsolatedOrigin`):** Determining if a site *must* run in its own process (due to global policy, isolated origin policy, OAC, WebUI, sandboxing, PDF, etc.).
*   **Default SiteInstance Eligibility (`SiteInstanceImpl::CanBePlacedInDefaultSiteInstance`):** Determining if a site can share the default process for the profile or needs a site-specific one.
*   **BrowsingInstance Swapping (`ShouldSwapBrowsingInstancesForNavigation`):** Deciding if a navigation needs to break script connections and move to a new BrowsingInstance (driven by COOP, WebUI transitions, cross-profile navigations, etc.).
*   **SiteInstance Selection (`GetSiteInstanceForNavigation`):** The main entry point in `RenderFrameHostManager` that orchestrates the above checks to select or create the appropriate `SiteInstance` for a navigation.
*   **Process Selection/Locking:** Assigning a `SiteInstance` to a `RenderProcessHost` and locking the process if necessary (`ShouldLockProcessToSite`).
*   **Commit Validation (`ValidateDidCommitParams`):** Final browser-side check to ensure the renderer process committing the navigation is allowed to host the claimed origin.

## 4. Potential Logic Flaws & VRP Relevance

Site Isolation bypasses often involve tricking the browser into placing content from different origins into the same process, or exploiting inconsistencies in how special URL schemes inherit security contexts.

*   **Incorrect Site/Origin Classification:** Errors in `SiteInfo::Create` or helpers like `GetSiteForURLInternal` misclassifying a URL's site, leading to incorrect process placement.
    *   **VRP Pattern (javascript:/data: URL Origin Confusion):** Ambiguity or incorrect handling of origins derived from `javascript:` or `data:` URLs, especially when opened in new windows/tabs from cross-origin contexts. (VRP: `40059251`).
*   **Isolation Bypass via Special Schemes/Contexts:** Specific URL schemes or contexts not being correctly isolated according to the standard model.
    *   **VRP Pattern (`blob:` URL Bypass):** `blob:` URLs potentially inheriting incorrect origins or failing isolation checks, allowing cross-site data access. (VRP2.txt#1761 - SOP/Site Iso bypass, #12127 - Opaque origin issue).
    *   **VRP Pattern (`filesystem:` URL Bypass):** `filesystem:` URLs created by one origin being accessible by another origin if checks in the browser process (`FileSystemURLLoaderFactory`, `ChildProcessSecurityPolicyImpl::CanRequestURL`) are insufficient. (VRP2.txt#6261).
    *   **VRP Pattern (SharedWorker):** Cross-origin sites potentially accessing the same SharedWorker using `data:` URLs due to incorrect origin checks. (VRP2.txt#16560). See [worker_threads.md](worker_threads.md).
    *   **VRP Pattern (Reader Mode):** Loading untrusted content (images, videos, potentially script via sanitization bypass) in the `chrome-distiller://` origin, potentially allowing SOP bypass if the distiller origin can interact with other origins inappropriately (e.g., via `window.open` if not properly restricted). (VRP2.txt#6759).
*   **Process Lock Errors:** Mistakes in `ShouldLockProcessToSite` or applying the lock in `LockProcessIfNeeded` failing to restrict a process correctly. Incorrect comparisons in `IsSuitableHost` / `ProcessLock::operator==`.
*   **WebUI Privilege Issues:** Incorrect process reuse allowing non-WebUI sites to gain WebUI privileges, or Mojo bindings persisting across navigations.
    *   **VRP 40053875:** WebUI MojoJS bindings leak to subsequent sites.
*   **State Confusion during Navigation/Redirects/Crashes:** Complex navigations or error conditions leading to inconsistent state being used for isolation decisions.
    *   **VRP Pattern (Popup Timing - VRP 40059251):** Transient inconsistencies where browser-side checks might use the opener's less restrictive `ProcessLock` before the popup commits to its final, potentially sandboxed/opaque origin state. See Section 5.
    *   **VRP Pattern (Crash Recovery):** URL spoofing or incorrect process allocation after a renderer crash. (VRP: `40057561`).
    *   **VRP Pattern (Data URL + Tab Restore):** Attacker-controlled `data:` URL loading in the wrong process after tab restore (VRP2.txt#11667).
*   **Information Leaks Across BrowsingInstances:** Incorrect logic in `ShouldSwapBrowsingInstancesForNavigation` or proxy handling might leak information (like `window.length`).
    *   **VRP 40059056:** Leaking window.length without opener reference.
*   **Policy Bypass:** Incorrectly evaluating command-line flags (`--site-per-process`) or enterprise policies (`IsIsolatedOrigin`) leading to insufficient isolation.
*   **OAC/COOP/Sandboxing Misconfiguration:** Bugs in how `UrlInfo` represents these states or how `SiteInfo` interprets them breaking expected isolation.
    *   **VRP 40056434:** crossOriginIsolated bypass (related to COOP/COEP enforcement).
    *   **VRP Pattern (Context Reuse - Sandboxed `about:blank`):** Sandboxed `allow-same-origin` iframe navigated from initial `about:blank` reusing the non-sandboxed context's type system (VRP2.txt#6788). See [iframe_sandbox.md](iframe_sandbox.md).

## 5. Popup Creation and Commit Validation (VRP 40059251 Analysis)

When a frame (`opener`) creates a new window using `window.open()` (without `noopener`), the process involves several steps with potential security implications related to process allocation and origin validation:

1.  **Initial SiteInstance Assignment (`WebContentsImpl::CreateNewWindow`):** The new popup initially inherits the *opener's* `SiteInstance`, potentially placing it in the opener's process context transiently.
2.  **Navigation and Commit:** The popup navigates (e.g., to `about:blank`, `data:`, `javascript:`). The browser's `NavigationRequest` calculates the correct final `SiteInfo` (e.g., opaque origin for `data:`) and target process based on Site Isolation rules.
3.  **Potential Timing Gap / "Browser-Side Confusion":** A vulnerability window exists between the initial assignment to the opener's `SiteInstance` (and its `ProcessLock`) and the final commit establishing the correct security context. Browser-side checks (e.g., `ChildProcessSecurityPolicyImpl::CanAccessDataForOrigin`) performed during this window might incorrectly use the opener's `ProcessLock` instead of the popup's intended final context. VRP 40059251 likely exploited such a timing issue with `javascript:` or `data:` URLs.
4.  **Final Commit Validation (`RenderFrameHostImpl::ValidateDidCommitParams`):** This function acts as the final safeguard. It verifies that the *committing* process (with its *current* `ProcessLock`) is allowed to host the origin claimed by the renderer (`params.origin`) and that this matches the browser's expected `origin_to_commit`. This *should* prevent exploits based on the transient state, but edge cases or flaws in this validation could lead to bypasses.

## 6. Areas Requiring Further Investigation
*   **Special Scheme Isolation:** Systematically audit the origin inheritance, SiteInstance assignment, process allocation, and commit validation logic for `data:`, `blob:`, `filesystem:`, `javascript:`, `about:blank`, and `about:srcdoc` URLs, especially when initiated cross-origin or in new windows/iframes. (VRP: `40059251`, VRP2.txt#1761, #6261, #11667).
*   **Commit Validation Robustness:** Deep dive into `ValidateDidCommitParams` and `ChildProcessSecurityPolicyImpl::CanCommitOriginAndUrl`. Are there scenarios (e.g., specific schemes, redirects, crashes) where `origin_to_commit` or the `ProcessLock` state used for validation is incorrect or can be manipulated?
*   **SharedWorker Isolation:** Investigate how SharedWorker script URLs (especially `data:` URLs) are keyed and if cross-origin sites can access the same worker instance (VRP2.txt#16560).
*   **Interaction with Sandboxing/COOP/OAC:** Ensure that navigations involving sandboxed frames, COOP/COEP policies, or OAC headers result in correct process allocation and context separation. Re-test sandbox context reuse (VRP2.txt#6788).
*   **State Management during Navigation/Crashes:** Audit state consistency (SiteInstance, ProcessLock, NavigationEntry) during complex navigations, redirects, and especially crash recovery flows (VRP: `40057561`).
*   **File Scheme Handling:** Although `file:` URLs are generally isolated, re-verify interactions, especially concerning default SiteInstance usage (`CanBePlacedInDefaultSiteInstance`).

## 7. Related VRP Reports (Examples)
*   VRP: `40059251` (Origin confusion for javascript/data URLs in popups)
*   VRP: `40053875` (WebUI MojoJS bindings leak)
*   VRP: `40056434` (crossOriginIsolated bypass)
*   VRP: `40059056` (window.length leak)
*   VRP: `40057561` (URL Spoof after crash)
*   VRP2.txt#1761 (`blob:` URL SOP/Site Iso bypass)
*   VRP2.txt#6261 (`filesystem:` URL Site Iso bypass)
*   VRP2.txt#16560 (`data:` URL SharedWorker cross-origin access)
*   VRP2.txt#6759 (Reader Mode potential isolation issues)
*   VRP2.txt#11667 (`data:` URL process misallocation after tab restore)
*   VRP2.txt#6788 (Sandboxed iframe `about:blank` context reuse)

## 8. Cross-References
*   [site_instance.md](site_instance.md)
*   [browsing_instance.md](browsing_instance.md)
*   [site_info.md](site_info.md)
*   [url_info.md](url_info.md)
*   [render_process_host.md](render_process_host.md)
*   [navigation.md](navigation.md)
*   [coop_coep.md](coop_coep.md)
*   [process_lock.md](process_lock.md)
*   [iframe_sandbox.md](iframe_sandbox.md)
*   [worker_threads.md](worker_threads.md) (Shared Workers)
