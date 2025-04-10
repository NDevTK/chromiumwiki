# SiteInfo

## 1. Component Focus
*   **Functionality:** Represents the security principal ("site") associated with a URL. Contains properties determining process allocation, isolation characteristics, and security policies applicable to content from that site. Created based on `UrlInfo` and `IsolationContext`.
*   **Key Logic:** Classifying URLs into sites (`GetSiteForURLInternal`), determining the appropriate process lock URL (`DetermineProcessLockURL`), evaluating isolation requirements (OAC, COOP, sandboxing, dedicated process policies), computing the `AgentClusterKey`, and packaging these into a `SiteInfo` object via `CreateInternal`.
*   **Core Files:**
    *   `content/browser/site_info.cc`/`.h`
    *   `content/browser/url_info.h` (Input)
    *   `content/browser/isolation_context.h` (Input)

## 2. Key Concepts & Interactions
*   **Input:** Created from a `UrlInfo` (carrying URL, initiator origin, isolation headers like OAC/COOP, sandboxing flags) and an `IsolationContext` (carrying BrowsingInstance state, browser context, global policies).
*   **Output:** Produces a `SiteInfo` object containing:
    *   `site_url()`: The canonical URL representing the site (scheme + eTLD+1, or origin for isolated sites, or special values for data:/blob:/error pages). Used for logical grouping.
    *   `process_lock_url()`: The URL used to lock the process. Often the same as `site_url`, but can differ (e.g., for some WebUI).
    *   Isolation Flags: `requires_origin_keyed_process`, `is_sandboxed`, `does_site_request_dedicated_process_for_coop`, `web_exposed_isolation_info`, etc.
    *   Other Flags: `is_guest`, `is_pdf`, `is_jit_disabled`, etc.
    *   `storage_partition_config()`: Determines cookie/storage isolation.
    *   `agent_cluster_key()`: Determines Agent Cluster boundaries.
*   **Usage:** The resulting `SiteInfo` is used by `BrowsingInstance::GetSiteInstanceForURL` to select or create the correct `SiteInstance` and by `SiteInstanceImpl` to configure process locking and behavior.

## 3. Potential Logic Flaws & VRP Relevance (Summary - See site_isolation.md for details)
*   **Incorrect Site/Origin Determination:** Bugs in `GetSiteForURLInternal` or `DetermineProcessLockURL` classifying special schemes (`data:`, `blob:`, `filesystem:`, `file:`, WebUI), handling opaque origins, or applying effective URLs incorrectly. (Related to VRP: `40059251`; VRP2.txt#173, #542, etc.).
*   **Incorrect Isolation Flag Determination:** Bugs in `CreateInternal` evaluating OAC headers (`url_info.requests_origin_agent_cluster_by_header`), COOP headers (`url_info.requests_coop_isolation`), sandboxing flags (`url_info.is_sandboxed`), or global/embedder policies (`RequiresDedicatedProcess`) leading to wrong process placement or insufficient isolation.
*   **Inconsistent State:** Mismatches between `site_url` and `process_lock_url` or other flags leading to unexpected behavior.

## 4. Functions and Methods (Key Ones for Site Definition)
*   **`SiteInfo::Create`**: Public entry point (UI thread only). Calls `CreateInternal`.
*   **`SiteInfo::CreateInternal`**: Core private factory method. Orchestrates calls to determine all `SiteInfo` properties based on `UrlInfo` and `IsolationContext`.
*   **`SiteInfo::DetermineProcessLockURL`**: Determines the URL used for process locking. Uses `GetPossiblyOverriddenOriginFromUrl` and handles special WebUI cases.
*   **`SiteInfo::GetSiteForURLInternal`**: Determines the canonical `site_url`. Uses `GetPossiblyOverriddenOriginFromUrl`, handles effective URLs, isolated origins, sandboxed data/blob URLs, and special schemes.
*   **`SiteInfo::GetPossiblyOverriddenOriginFromUrl`**: Helper to get the correct origin to use for site/lock calculation, handling `data:`, `about:blank`, and potentially others based on `UrlInfo::origin`.
*   **`SiteInfo::RequiresDedicatedProcess`**: Checks multiple factors (global policies, isolation requests, WebUI, schemes, sandboxing, embedder callbacks) to see if the site needs its own process.
*   **`SiteInfo::ShouldLockProcessToSite`**: Determines if the dedicated process (if required) should be strictly locked to this site.

## 5. Areas Requiring Further Investigation (Highlights)
*   **Special Scheme Handling:** Thoroughly audit how `GetSiteForURLInternal` and `DetermineProcessLockURL` handle `data:`, `blob:`, `filesystem:`, `file:`, `about:srcdoc`, `javascript:`, and WebUI URLs under all conditions (sandboxed, effective URLs, isolation policies).
*   **Isolation Flag Logic:** Audit how `requires_origin_keyed_process` and `does_site_request_dedicated_process_for_coop` flags are derived from `UrlInfo` and `IsolationContext` policies within `CreateInternal`.
*   **Effective URL Interaction:** How does `should_use_effective_urls=true` affect `site_url` calculation vs. `process_lock_url` calculation?
*   **AgentClusterKey Calculation:** How is `agent_cluster_key_` determined, especially its interaction with COOP/COEP and OAC?

## 6. Related VRP Reports
*(See [site_isolation.md](site_isolation.md) for a comprehensive list related to process model failures)*

*(See also: [url_info.md](url_info.md), [site_instance.md](site_instance.md), [site_isolation.md](site_isolation.md), [browsing_instance.md](browsing_instance.md))*
