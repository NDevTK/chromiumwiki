# ProcessLock

## 1. Component Focus
*   **Functionality:** Represents the security principal to which a `RenderProcessHost` is locked. This determines which sites or origins the process is allowed to host content from. A stricter lock (e.g., locked to `https://a.com`) prevents the process from hosting content from unrelated sites (e.g., `https://b.com`).
*   **Key Logic:** Encapsulating the relevant security properties from a `SiteInfo` object, providing factory methods (`Create`, `FromSiteInfo`, `CreateAllowAnySite`), and defining equality comparison (`operator==` via `SiteInfo::ProcessLockCompareTo`). The lock is set via `RenderProcessHostImpl::SetProcessLock`, typically triggered by `SiteInstanceImpl::LockProcessIfNeeded`.
*   **Core Files:**
    *   `content/browser/process_lock.cc`/`.h`
    *   `content/browser/site_info.cc`/`.h` (Provides underlying data)
    *   `content/browser/site_instance_impl.cc`/`.h`: Calls `LockProcessIfNeeded`.
    *   `content/browser/child_process_security_policy_impl.cc`: Uses the lock for security checks.

## 2. Key Concepts & Interactions
*   **Based on SiteInfo:** A `ProcessLock` is essentially derived from a `SiteInfo` object, containing the subset of `SiteInfo` fields relevant for process isolation decisions.
*   **RenderProcessHost Association:** Each `RenderProcessHost` has an associated `ProcessLock` stored in its `ChildProcessSecurityPolicyImpl::SecurityState`. It is set via `RenderProcessHostImpl::SetProcessLock`.
*   **Lock Types:**
    *   **Invalid:** Default state, no lock applied.
    *   **AllowAnySite:** Process is not locked to a specific site but has associated `StoragePartitionConfig` and `WebExposedIsolationInfo`. Can host sites that don't require dedicated processes. Created via `CreateAllowAnySite`.
    *   **LockedToSite:** Process is locked to a specific `process_lock_url_` and associated properties (origin-keying, sandboxing, COI, etc.). Can only host content matching this lock. Created via `Create` or `FromSiteInfo`.
*   **Suitability Checks:** Used heavily in `RenderProcessHostImpl::IsSuitableHost` to compare the existing process's lock (`host->GetProcessLock()`) with the lock expected for a target URL/SiteInfo (`ProcessLock::FromSiteInfo(target_site_info)`).
*   **Security Enforcement:** Used by `ChildProcessSecurityPolicyImpl` (e.g., in `PerformJailAndCitadelChecks`) to verify if a process is allowed to access data for a given origin based on its lock. This is a crucial part of commit validation (`RenderFrameHostImpl::ValidateDidCommitParams`).
*   **Timing:** The `ProcessLock` is typically set when a `SiteInstance` is first assigned a site or process. There can be a transient period (e.g., during popup creation without `noopener`) where a `FrameTree` is associated with a `SiteInstance` whose process still holds the lock of the *opener*, before the final navigation commits and the correct lock is potentially applied.

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
*   `storage_partition_config_`
*   `is_fenced_`
*   `agent_cluster_key_` (Includes COI key)

**Fields *NOT* Compared (selected):**

*   `site_url_` (Ignored to handle effective URLs)
*   `is_jit_disabled_`, `are_v8_optimizations_disabled_`

Two `ProcessLock`s are equal if and only if all the compared fields match.

## 5. Potential Logic Flaws & VRP Relevance
*   **Incorrect Lock Creation:** Bugs in `SiteInfo::Create` leading to incorrect fields used in the lock.
*   **Incorrect Lock Comparison:** Flaws in `SiteInfo::ProcessLockCompareTo` or `MakeProcessLockComparisonKey` (e.g., missing a critical field, comparing incorrectly) causing incorrect process reuse decisions in `IsSuitableHost`.
*   **Inconsistent Lock Application vs. Comparison:** Discrepancies between fields used for comparison and the actual state enforced by the lock.
*   **Timing Issues (VRP 40059251):** Vulnerabilities might arise if security checks (`CanAccessDataForOrigin`) read the `ProcessLock` during a transient state (e.g., popup creation inheriting opener's lock) before the final, correct lock is applied during commit validation. The final validation (`ValidateDidCommitParams`) relies on the browser having the *correct* expected lock state for the committed URL at that point.

*(See also: [site_info.md](site_info.md), [render_process_host.md](render_process_host.md), [site_isolation.md](site_isolation.md), [navigation.md](navigation.md), [site_instance.md](site_instance.md))*