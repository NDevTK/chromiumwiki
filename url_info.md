# UrlInfo

## 1. Component Focus
*   **Functionality:** Represents comprehensive information about a URL being considered during navigation or for SiteInstance selection. It aggregates the URL itself along with security-critical properties derived from various sources like navigation parameters, response headers (COOP/COEP/OAC), and context (initiator origin, sandbox state, iframe attributes).
*   **Key Data:** `url`, `origin` (potentially overridden), `storage_partition_config`, `is_sandboxed`, `unique_sandbox_id`, `origin_isolation_request` (from OAC header), `is_coop_isolation_requested` (from COOP header hint), `web_exposed_isolation_info` (from COOP/COEP headers), `cross_origin_isolation_key` (from COOP/COEP/DIP), `is_pdf`, etc.
*   **Core Files:**
    *   `content/browser/url_info.h` (Definition)
    *   Often constructed/updated within `NavigationRequest` or other navigation-related logic (e.g., `FrameTreeNode`).

## 2. Key Concepts & Interactions
*   **Input to SiteInfo:** `UrlInfo` objects are the primary input to `SiteInfo::Create`, which translates these properties into a `SiteInfo` used for process model decisions (SiteInstance selection, process locking).
*   **Navigation Lifecycle:** Constructed using `UrlInfoInit` early in navigation (e.g., in `NavigationRequest::CreateRendererInitiated`), potentially based only on the target URL and initiator context. It is updated as more information becomes available, particularly after receiving response headers (`NavigationRequest::OnResponseStarted` often updates COOP/COEP/OAC related fields based on parsed headers).
*   **Security Properties:** Carries essential flags derived from headers (like COOP/COEP influencing `web_exposed_isolation_info` and `cross_origin_isolation_key`, OAC influencing `origin_isolation_request`) and context (like `is_sandboxed` derived from iframe attributes or CSP, `origin` potentially holding initiator/base URL).

## 3. Key Data Fields Analysis
*   **`url`**: The target URL of the navigation or context.
*   **`origin_isolation_request`**: Enum (`kDefault`, `kNone`, `kOriginAgentClusterByHeader`, `kRequiresOriginKeyedProcessByHeader`) capturing the state requested by the `Origin-Agent-Cluster` HTTP header. Used by `SiteInfo::Create` to determine `requires_origin_keyed_process`.
*   **`is_coop_isolation_requested`**: Boolean flag set if the COOP header suggests a site isolation hint should be applied. Used by `SiteInfo::Create` to potentially set `does_site_request_dedicated_process_for_coop` on the resulting `SiteInfo`.
*   **`is_prefetch_with_cross_site_contamination`**: Indicates potential timing side-channels via prefetch. Can trigger BrowsingInstance swaps in `ShouldSwapBrowsingInstancesForNavigation`.
*   **`origin`**: Usually `std::nullopt`. Set for specific cases like `data:` URLs (holds opaque origin with nonce), `about:blank` (holds initiator origin), or `LoadDataWithBaseURL` (holds base URL origin). Used by `GetPossiblyOverriddenOriginFromUrl`.
*   **`is_sandboxed` / `unique_sandbox_id`**: Reflects whether the context is subject to origin-restricting sandbox flags (from `<iframe>` or CSP). `unique_sandbox_id` is used for per-document isolation of sandboxed iframes. Crucial input for `SiteInfo` security properties.
*   **`storage_partition_config`**: Specifies the storage partition, critical for cookie/storage isolation boundaries. If not set, defaults are used.
*   **`web_exposed_isolation_info`**: Holds the `WebExposedIsolationInfo` (derived from COOP/COEP headers), indicating if the context is cross-origin isolated and/or an isolated application. Essential for process model decisions and SiteInstance compatibility checks (`IsSiteInstanceCompatibleWithWebExposedIsolation`). Defaults to non-isolated if not set (e.g., before headers are received).
*   **`is_pdf`**: Flag indicating PDF content, which forces process isolation.
*   **`common_coop_origin`**: Used internally for managing CoopRelatedGroups, set when COOP is `restrict-properties` or `same-origin`.
*   **`cross_origin_isolation_key`**: Newer mechanism related to `DocumentIsolationPolicy` and intended for future COOP/COEP integration. Used to create the `AgentClusterKey` on the `SiteInfo`.

## 4. Potential Logic Flaws & VRP Relevance
*   **Incorrect Property Calculation:** Errors in deriving flags like `is_sandboxed`, `web_exposed_isolation_info`, `origin_isolation_request`, or `is_coop_isolation_requested` from the actual navigation context (e.g., misinterpreting headers, CSP, iframe attributes, MHTML context). This leads to incorrect `SiteInfo` and potentially wrong process allocation.
*   **State Desynchronization:** If `UrlInfo` is not correctly updated during redirects or other navigation events (especially header parsing in `NavigationRequest`), stale or incorrect information could be passed to `SiteInfo::Create`, bypassing isolation.
*   **Missing Information:** Failure to propagate necessary information (e.g., initiator origin for `about:blank`, correct sandbox flags, COOP/COEP headers) into the `UrlInfo` object at the right time.

## 5. Functions and Methods
*   **`UrlInfoInit` / Constructors:** Primary way `UrlInfo` objects are created, using a builder pattern (`UrlInfoInit(url).WithOrigin(...)`).
*   **`UrlInfo::CreateForTesting`**: Test utility.
*   Various methods within `NavigationRequest` that populate or update `UrlInfo` fields, such as `NavigationRequest::CreateRendererInitiated`, `NavigationRequest::OnResponseStarted`, `NavigationRequest::SetUrlInfoOverrides`.

## 6. Areas Requiring Further Investigation
*   How and when exactly are the various fields (especially security flags like `is_sandboxed`, `web_exposed_isolation_info`, OAC/COOP requests) populated during the `NavigationRequest` lifecycle? Trace the flow from header parsing (`NavigationResponseProcessor`) to `UrlInfo` updates.
*   How is the `origin` field determined, especially for opaque origins (`data:`, `about:blank`, `srcdoc`)? Verify alignment with `GetPossiblyOverriddenOriginFromUrl`.
*   How are redirects handled in terms of updating the contained `UrlInfo` within `NavigationRequest::OnRequestRedirected`? Are all relevant fields correctly reset or updated?
*   How does `NavigationRequest::SetUrlInfoOverrides` interact with the base `UrlInfo`?

## 7. Related VRP Reports
*   *(Indirectly related to VRPs caused by incorrect SiteInstance selection, if the root cause is faulty UrlInfo construction)*

*(See also: [site_info.md](site_info.md), [navigation_request.md](navigation_request.md), [iframe_sandbox.md](iframe_sandbox.md), [coop_coep.md](coop_coep.md), [site_instance.md](site_instance.md))*
