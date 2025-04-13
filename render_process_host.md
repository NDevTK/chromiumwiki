# RenderProcessHost

## 1. Component Focus
*   **Functionality:** Represents the browser-side handle to a single renderer process. Manages the process lifecycle (launching, termination), communication channel (IPC, Mojo), security policies (process lock, permissions via `ChildProcessSecurityPolicyImpl`), and resource management (priority, memory).
*   **Key Logic:** Process creation (`Init`), managing the Mojo/IPC channel, applying process lock (`SetProcessLock`), checking process suitability for reuse (`IsSuitableHost`), selecting/creating processes based on policy and availability (`GetProcessHostForSiteInstance`), managing process priority (`UpdateProcessPriority`), handling process death (`ProcessDied`), managing keep-alive counts.
*   **Core Files:**
    *   `content/browser/renderer_host/render_process_host_impl.cc`/`.h`
    *   `content/public/browser/render_process_host.h` (Public API)
    *   `content/browser/child_process_launcher.*` (Handles actual process spawning)
    *   `content/browser/process_lock.*` (Defines process security lock)
    *   `content/browser/process_reuse_policy.h` (Defines reuse policies)

## 2. Key Concepts & Interactions
*   **One per Process:** Typically, one RPH instance per active renderer process.
*   **Process Lifecycle:** Created (`CreateRenderProcessHost`), initialized (`Init`), cleaned up (`Cleanup`). `ChildProcessLauncher` handles OS spawning.
*   **Communication:** Owns Mojo/IPC channel (`channel_`). Provides `mojom::Renderer`, `mojom::ChildProcess`.
*   **Security Context:** Associated with `BrowserContext`, `StoragePartition`. Security managed by `ChildProcessSecurityPolicyImpl`.
*   **Process Lock:** See [process_lock.md](process_lock.md). Set via `SetProcessLock`, checked by `IsSuitableHost`. Critical for isolation.
*   **SiteInstances:** Hosts RFHs from `SiteInstance`s grouped into a `SiteInstanceGroup`.
*   **Priority:** OS process priority managed based on visibility, media, etc. (`UpdateProcessPriority`).
*   **Keep-Alive:** Ref-counts prevent premature shutdown.
*   **Process Reuse Policy:** Enum (`content::ProcessReusePolicy`) on `SiteInstance` guiding process selection. See [site_instance.md](site_instance.md).
*   **SiteProcessCountTracker:** Internal helper class (defined within `render_process_host_impl.cc`) used to track site-to-process associations for reuse strategies. See Section 3.1.

## 3. Process Selection / Reuse (`RenderProcessHostImpl::GetProcessHostForSiteInstance`)

Central logic for obtaining an RPH for a `SiteInstanceImpl`, reusing or creating as needed.

**Steps:**

1.  **Check Policy-Based Reuse:** Attempts reuse based on `site_instance->process_reuse_policy_`:
    *   `PROCESS_PER_SITE`: Calls `GetSoleProcessHostForSite`.
    *   `REUSE_PENDING_OR_COMMITTED_SITE_*`: Calls `FindReusableProcessHostForSiteInstance` (see Section 3.1).
2.  **Check Unmatched Service Worker:** If no process found (and not DEFAULT policy for new worker), calls `UnmatchedServiceWorkerProcessTracker::MatchWithSite`.
3.  **Check Embedder Preference:** If no process found, consults `ContentBrowserClient::ShouldTryToUseExistingProcessHost`. If true, calls `GetExistingProcessHost` (finds random suitable host).
4.  **Check Spare Process:** If no process found, calls `SpareRenderProcessHostManager::MaybeTakeSpare`.
5.  **Check Process Limit:** If no process found *and* `IsProcessLimitReached()`, calls `GetExistingProcessHost` (finds random suitable host).
6.  **Suitability Double-Check:** If a process *was* found for reuse, verifies with `MayReuseAndIsSuitable`. Triggers `NOTREACHED` on failure.
7.  **Create New Process:** If no suitable existing process found, calls `CreateRenderProcessHost`.
8.  **Post-Selection:** Initializes process if needed, warms up spare, registers unmatched workers, checks storage partition.

### 3.1 Finding Reusable Hosts for `REUSE_PENDING_OR_COMMITTED_SITE_*` (`FindReusableProcessHostForSiteInstance`)

Helper function used when the `ProcessReusePolicy` is one of the `REUSE_PENDING_OR_COMMITTED_SITE_*` values. It queries internal `SiteProcessCountTracker` instances to find suitable processes.

**Logic:**

1.  Checks if the site should be tracked (`ShouldFindReusableProcessHostForSite`).
2.  Queries `SiteProcessCountTracker` for *pending* navigations to the target `SiteInfo`. Populates lists of suitable foreground/background hosts based on `MayReuseAndIsSuitable` and resource checks (`IsBelowReuseResourceThresholds`).
3.  If no *foreground* pending host found, queries `SiteProcessCountTracker` for *committed* frames for the target `SiteInfo`, populating the lists similarly.
4.  If still no host found and delayed shutdown is supported, queries `SiteProcessCountTracker` for processes pending *delayed shutdown* that previously hosted the site.
5.  Selects a host: Prioritizes foreground hosts, then background hosts. Picks randomly within the chosen category.
6.  Returns the selected host or `nullptr`.

**SiteProcessCountTracker:**
This internal helper class (defined within `render_process_host_impl.cc`) maintains mappings (`SiteInfo` -> Map<`ChildProcessId`, Count>) for pending navigations, committed frames, and delayed shutdown processes within a `BrowserContext`. It allows `FindRenderProcessesForSiteInstance` (called by `FindReusableProcessHostForSiteInstance`) to efficiently retrieve potential candidate processes associated with a specific `SiteInfo`, before applying suitability and resource checks. It increments/decrements counts as frames/navigations are added/removed and cleans up when processes are destroyed.

## 4. Process Suitability Check (`RenderProcessHostImpl::IsSuitableHost`)

Checks if an *existing* RPH (`host`) is suitable for a target `site_info`. Called by `SiteInstanceImpl::IsSuitableForUrlInfo` and internally by `GetProcessHostForSiteInstance`.

**Logic:** Returns `false` (unsuitable) if *any* mismatch is found between `host` and `site_info` regarding: BrowserContext, Guest Status, JIT Policy, V8 Opt Policy, PDF Status, Storage Partition, V8 Flag Override Policy, WebUI Bindings, Process Lock (`ProcessLock::operator==`), COI compatibility, pending "siteless" navigations (if target needs dedication), or embedder policy (`ContentBrowserClient::IsSuitableHost`). Otherwise returns `true`.

*(See `process_lock.md` for lock comparison details)*

## 5. Potential Logic Flaws & VRP Relevance
*   **Incorrect Reuse Policy Application/Logic**: Flaws in `GetProcessHostForSiteInstance` or `FindReusableProcessHostForSiteInstance`.
*   **Incorrect Suitability Check (`IsSuitableHost`)**: Reusing an unsuitable process.
*   **Tracking Errors (`SiteProcessCountTracker`, `UnmatchedServiceWorkerProcessTracker`)**: Incorrectly identifying reusable processes or memory leaks in trackers.
*   **Process Limit Bypass**: Incorrectly calculating the limit or failing to reuse when the limit is hit.
*   **Spare Process Misuse**: Taking the spare when not appropriate, or failing to take it when needed.
*   **Unmatched Worker Logic**: Bugs in tracking or matching unmatched service worker processes.
*   **Keep-Alive/Shutdown Logic Errors**: Leading to premature termination or resource exhaustion.
*   **Priority Management Issues**: Performance/responsiveness problems.
*   **IPC Handling Bugs**: Vulnerabilities in RPH message handlers.

## 6. Functions and Methods (Key Ones for Process Selection/Lifecycle)
*   **`RenderProcessHostImpl::GetProcessHostForSiteInstance`**: Central selection/creation logic.
*   **`RenderProcessHostImpl::GetSoleProcessHostForSite`**: Finds process for `PROCESS_PER_SITE`.
*   **`RenderProcessHostImpl::FindReusableProcessHostForSiteInstance`**: Finds process for `REUSE_PENDING_OR_COMMITTED_SITE_*`.
*   **`RenderProcessHostImpl::GetExistingProcessHost`**: Finds a random suitable existing host.
*   **`RenderProcessHostImpl::IsSuitableHost`**: Checks RPH suitability for a SiteInfo.
*   **`RenderProcessHostImpl::MayReuseAndIsSuitable`**: Combines `MayReuseHost` and `IsSuitableHost`.
*   **`RenderProcessHostImpl::Init`**: Launches the process.
*   **`RenderProcessHostImpl::Cleanup`**: Initiates shutdown.
*   **`RenderProcessHostImpl::SetProcessLock`**: Applies security lock.
*   **`ProcessLock::Create` / `ProcessLock::FromSiteInfo` / `ProcessLock::operator==`**: Lock creation/comparison.
*   **`SiteProcessCountTracker::FindRenderProcessesForSiteInstance`**: Internal helper for finding hosts based on site tracking.

## 7. Areas Requiring Further Investigation
*   Detailed analysis of `SiteProcessCountTracker` logic (how counts are maintained).
*   Logic within `IsBelowReuseResourceThresholds`.
*   The workings of `UnmatchedServiceWorkerProcessTracker`.
*   Interaction with Spare RPH manager (`SpareRenderProcessHostManager`).
*   How exactly the `ProcessReusePolicy` is set on a `SiteInstance` during navigation lifecycle.

## 8. Related VRP Reports
*   VRPs related to incorrect process reuse leading to isolation bypass.
*   VRPs related to process limit handling errors.
*   VRPs involving service workers potentially running in incorrect processes.

*(See also: [site_instance.md](site_instance.md), [site_info.md](site_info.md), [site_isolation.md](site_isolation.md), [browsing_instance.md](browsing_instance.md), [child_process_security_policy_impl.md](child_process_security_policy_impl.md), [process_lock.md](process_lock.md), [navigation.md](navigation.md))*
