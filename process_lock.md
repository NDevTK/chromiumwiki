# ProcessLock

## 1. Component Focus
*   **Functionality:** Represents the security principal to which a `RenderProcessHost` is locked. This determines which sites or origins the process is allowed to host content from. A stricter lock (e.g., locked to `https://a.com`) prevents the process from hosting content from unrelated sites (e.g., `https://b.com`).
*   **Key Logic:** Encapsulating the relevant security properties from a `SiteInfo` object, providing factory methods (`Create`, `FromSiteInfo`, `CreateAllowAnySite`), and defining equality comparison (`operator==` via `SiteInfo::ProcessLockCompareTo`).
*   **Core Files:**
    *   `content/browser/process_lock.cc`/`.h`
    *   `content/browser/site_info.cc`/`.h` (Provides underlying data)

## 2. Key Concepts & Interactions
*   **Based on SiteInfo:** A `ProcessLock` is essentially derived from a `SiteInfo` object, containing the subset of `SiteInfo` fields relevant for process isolation decisions.
*   **RenderProcessHost Association:** Each `RenderProcessHost` has an associated `ProcessLock`, set via `RenderProcessHostImpl::SetProcessLock`.
*   **Lock Types:**
    *   **Invalid:** Default state, no lock applied.
    *   **AllowAnySite:** Process is not locked to a specific site but has associated `StoragePartitionConfig` and `WebExposedIsolationInfo`. Can host sites that don't require dedicated processes. Created via `CreateAllowAnySite`.
    *   **LockedToSite:** Process is locked to a specific `process_lock_url_` and associated properties (origin-keying, sandboxing, COI, etc.). Can only host content matching this lock. Created via `Create` or `FromSiteInfo`.
*   **Suitability Checks:** Used heavily in `RenderProcessHostImpl::IsSuitableHost` to compare the existing process's lock (`host->GetProcessLock()`) with the lock expected for a target URL/SiteInfo (`ProcessLock::FromSiteInfo(target_site_info)`).
*   **Security Enforcement:** Used by `ChildProcessSecurityPolicyImpl` (e.g., in `PerformJailAndCitadelChecks`) to verify if a process is allowed to access data for a given origin based on its lock.

## 3. ProcessLock Creation

*   **`ProcessLock::Create(const IsolationContext& isolation_context, const UrlInfo& url_info)`:**
    *   Primary factory method used to determine the *expected* lock for a URL/context.
    *   Calls `SiteInfo::Create` or `SiteInfo::CreateOnIOThread` internally.
*   **`ProcessLock::FromSiteInfo(const SiteInfo& site_info)`:**
    *   Directly constructs a lock from an *existing* `SiteInfo`.
*   **`ProcessLock::CreateAllowAnySite(...)`:**
    *   Creates a lock for processes not restricted to a specific site.

## 4. ProcessLock Comparison (`operator==` and `SiteInfo::ProcessLockCompareTo`)

Determines if two `ProcessLock` objects represent the same security context. The `ProcessLock::operator==` delegates to `SiteInfo::ProcessLockCompareTo`, which compares a specific subset of `SiteInfo` fields bundled into a tuple by `SiteInfo::MakeProcessLockComparisonKey`.

**Fields Compared for Lock Equivalence:**

*   `process_lock_url_`
*   `requires_origin_keyed_process_` (OAC status)
*   `is_sandboxed_`
*   `unique_sandbox_id_`
*   `is_pdf_`
*   `is_guest_`
*   `web_exposed_isolation_info_` (COOP/COEP info)
*   `web_exposed_isolation_level_` (COOP/COEP info)
*   `storage_partition_config_`
*   `is_fenced_`
*   `agent_cluster_key_` (Includes COI key)

**Fields *NOT* Compared:**

*   `site_url_` (Ignored to handle effective URLs)
*   `is_jit_disabled_` (Excluded due to TODO about crashes)
*   `are_v8_optimizations_disabled_` (Excluded due to TODO about crashes)
*   `does_site_request_dedicated_process_for_coop_` (Doesn't affect lock definition itself)

Two `ProcessLock`s are equal if and only if all the compared fields match.

## 5. Potential Logic Flaws & VRP Relevance
*   **Incorrect Lock Creation:** Bugs in `SiteInfo::Create` leading to incorrect fields used in the lock.
*   **Incorrect Lock Comparison:** Flaws in `SiteInfo::ProcessLockCompareTo` or `MakeProcessLockComparisonKey` (e.g., missing a critical field, comparing incorrectly) causing incorrect process reuse decisions in `IsSuitableHost`.
*   **Inconsistent Lock Application vs. Comparison:** Discrepancies between fields used for comparison and the actual state enforced by the lock.

*(See also: [site_info.md](site_info.md), [render_process_host.md](render_process_host.md), [site_isolation.md](site_isolation.md))*