# UrlInfo

This page details the `UrlInfo` struct and its role in site isolation.

## Core Concepts

The `UrlInfo` struct packages a `GURL` together with extra state required to make `SiteInstance`/process allocation decisions. Errors in populating or interpreting this state can lead to incorrect isolation decisions.

### Key Areas of Concern

-   `origin_isolation_request`: Incorrectly identifying origin isolation requests can lead to processes being shared when they should be isolated. This field is of type `UrlInfo::OriginIsolationRequest`, which is an enum with the following values: `kDefault`, `kNone`, `kOriginAgentClusterByHeader`, and `kRequiresOriginKeyedProcessByHeader`.
-   `is_coop_isolation_requested`: Misinterpreting COOP headers can result in sites not being isolated as expected.
-   `origin`: Overriding the origin incorrectly for special URLs (e.g., `data:`, `about:blank`) can lead to security bypasses.
-   `unique_sandbox_id`: Incorrectly assigning or handling sandbox IDs can break iframe isolation. This field is used when `is_sandboxed` is true to allow for per-document `SiteInfo` grouping.
-   `web_exposed_isolation_info`: Errors in determining the web-exposed isolation level can lead to cross-origin leaks. This field is of type `std::optional<WebExposedIsolationInfo>`.
-   `is_prefetch_with_cross_site_contamination`: Incorrectly handling prefetched resources with cross-site contamination can lead to security issues.
-   `is_sandboxed`: Incorrectly identifying if a URL is being loaded in a sandboxed frame can lead to security bypasses.
-   `storage_partition_config`: Incorrectly determining the `StoragePartitionConfig` can lead to data leaks or corruption.
-   `is_pdf`: Incorrectly identifying PDF content can lead to security issues.
-   `common_coop_origin`: Incorrectly determining the common COOP origin can lead to incorrect `BrowsingInstance` selection.
-   `cross_origin_isolation_key`: Incorrectly determining the `CrossOriginIsolationKey` can lead to security issues.

### Related Files

-   `content/browser/url_info.cc`
-   `content/browser/url_info.h`

### Functions and Methods

-   `UrlInfoInit`: This helper struct is used to initialize `UrlInfo` objects. Incorrect use of its `With...` methods can lead to misconfigured `UrlInfo` objects and subsequent security issues.
    -   The `UrlInfoInit` struct uses a builder pattern with methods like `WithOriginIsolationRequest`, `WithCOOPSiteIsolation`, `WithCrossSitePrefetchContamination`, `WithOrigin`, `WithSandbox`, `WithUniqueSandboxId`, `WithStoragePartitionConfig`, `WithWebExposedIsolationInfo`, `WithIsPdf`, `WithCommonCoopOrigin`, and `WithCrossOriginIsolationKey` to set the various fields of the `UrlInfo` struct.
-   `UrlInfo::IsIsolated()`: This method checks if the `UrlInfo` represents an isolated resource based on the `web_exposed_isolation_info` and `cross_origin_isolation_key` fields.
-   `UrlInfo::RequestsOriginKeyedProcess()`: This method determines if the `UrlInfo` requires an origin-keyed process, considering both explicit header requests and default isolation settings.
-   `UrlInfo::WriteIntoTrace()`: This method writes the `UrlInfo` data into a trace proto for debugging and analysis.
-   `UrlInfo::requests_default_origin_agent_cluster_isolation()`: This method checks if the `UrlInfo` is requesting the default origin agent cluster isolation.
-   `UrlInfo::requests_origin_agent_cluster_by_header()`: This method checks if the `UrlInfo` is requesting an origin-keyed agent cluster due to the `OriginAgentCluster` header.
-   `UrlInfo::requests_origin_keyed_process_by_header()`: This method checks if the `UrlInfo` is requesting an origin-keyed process due to the `OriginAgentCluster` header.
-   `UrlInfo::requests_coop_isolation()`: This method checks if the `UrlInfo` is requesting site isolation due to the `Cross-Origin-Opener-Policy` header.

### Further Analysis

-   The interaction between `UrlInfo` and different types of URLs, especially those with unique origins (e.g., blob URLs, filesystem URLs).
-   The impact of incorrect origin handling on cross-origin communication and data access.
-   The logic within `UrlInfo::RequestsOriginKeyedProcess` to ensure that origin-keyed processes are used correctly based on headers and default settings.
-   The usage of `UrlInfoInit` and its `With...` methods to ensure that all fields are correctly initialized.
-   The logic within `UrlInfo::IsIsolated` to ensure that isolated resources are correctly identified.
-   The logic within `UrlInfo::requests_default_origin_agent_cluster_isolation`, `UrlInfo::requests_origin_agent_cluster_by_header`, `UrlInfo::requests_origin_keyed_process_by_header`, and `UrlInfo::requests_coop_isolation` to ensure that isolation requests are correctly identified.
-   The interaction between the various fields in `UrlInfo` and how they collectively determine the isolation requirements for a given URL.
-   The handling of edge cases, such as when multiple isolation requests are present or when conflicting isolation requirements are encountered.

### Files Analyzed:

-   `chromiumwiki/url_info.md`
-   `content/browser/url_info.cc`
-   `content/browser/url_info.h`
