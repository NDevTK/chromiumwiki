# Site Isolation: A Focus on Finding Security Logic Issues

This document outlines my current understanding of site isolation in the Chromium project, based on my recent research of the codebase, with a particular focus on identifying potential security logic issues.

## Core Concepts

Site isolation is a security mechanism that aims to isolate web content from different sites into separate processes. This prevents malicious websites from accessing data or resources from other sites, even if they are running in the same browser instance. A failure in the logic that determines site isolation can lead to severe security vulnerabilities.

### File Locations

When referring to files in this document, use full paths relative to the Chromium root directory (e.g., `content/browser/renderer_host/navigation_request.cc`).

-   `content/browser/browsing_instance.cc`
-   `content/browser/site_instance_impl.cc`
-   `content/browser/child_process_security_policy_impl.cc`
-   `content/browser/service_worker/service_worker_process_manager.cc`
-   `content/browser/renderer_host/render_frame_host_impl.cc`
-   `content/browser/child_process_security_policy_unittest.cc`
-   `content/browser/renderer_host/render_frame_host_manager.cc`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/worker_host/shared_worker_service_impl.cc`
-   `content/browser/url_info.cc`
-   `content/browser/url_info.h`
-   `content/browser/shared_storage/shared_storage_render_thread_worklet_driver.cc`
-   `content/browser/site_instance_impl_unittest.cc`
-   `content/browser/site_per_process_oopsif_browsertest.cc`
-   `content/browser/site_info.cc`

### Key Components and Potential Security Concerns

1. **`UrlInfo`**: This struct packages a `GURL` together with extra state required to make SiteInstance/process allocation decisions. Errors in populating or interpreting this state can lead to incorrect isolation decisions. Key areas of concern include:
    -   `origin_isolation_request`: Incorrectly identifying origin isolation requests can lead to processes being shared when they should be isolated.
    -   `is_coop_isolation_requested`: Misinterpreting COOP headers can result in sites not being isolated as expected.
    -   `origin`: Overriding the origin incorrectly for special URLs (e.g., `data:`, `about:blank`) can lead to security bypasses.
    -   `unique_sandbox_id`: Incorrectly assigning or handling sandbox IDs can break iframe isolation.
    -   `web_exposed_isolation_info`: Errors in determining the web-exposed isolation level can lead to cross-origin leaks.
2. **`UrlInfoInit`**: This helper struct is used to initialize `UrlInfo` objects. Incorrect use of its `With...` methods can lead to misconfigured `UrlInfo` objects and subsequent security issues.
3. **`SiteInfo`**: This class represents the site of a URL and determines if two URLs belong to the same site. Logic errors here can lead to incorrect same-site/cross-site classifications, potentially allowing cross-site attacks.
4. **`SiteInstance`**: This class represents a group of documents that share the same process. Incorrectly assigning documents to `SiteInstance`s can break site isolation, allowing documents from different sites to share a process.
5. **`BrowsingInstance`**: This class represents a group of `SiteInstance`s that share the same browsing context. Errors in managing `BrowsingInstance`s can lead to inconsistent isolation decisions across tabs and windows, potentially creating vulnerabilities.
6. **`NavigationRequest`**: This class manages the navigation lifecycle and determines the `UrlInfo` for the navigation. Logic errors here can result in incorrect isolation decisions for navigations, potentially leading to security breaches.

### Process Allocation: Potential Vulnerabilities

The process allocation for a navigation is determined based on the `UrlInfo` object and the current state of the `BrowsingInstance`. The following factors are considered, each with potential security implications:

-   **Site Isolation**: Incorrectly handling site isolation requests (e.g., due to COOP) can lead to sites not being isolated when they should be.
-   **Origin Isolation**: Misinterpreting origin isolation requests (e.g., via the `Origin-Agent-Cluster` header) can result in origins not being isolated properly.
-   **Sandbox Flags**: Incorrectly handling sandbox flags can lead to sandboxed frames not being isolated as expected, potentially allowing them to escape the sandbox.
-   **Storage Partition**: Errors in determining the storage partition can lead to data leaks between different storage partitions.
-   **Web Exposed Isolation Info**: Incorrectly determining the web exposed isolation info can lead to cross-origin leaks.
-   **COOP**: Misinterpreting the Cross-Origin-Opener-Policy header can result in sites not being isolated as expected.

### Key Functions: Areas for Security Audits

-   **`GetPossiblyOverriddenOriginFromUrl`**: This function determines the origin to use for site and process lock URL computation. Incorrectly overriding the origin can lead to security bypasses.
-   **`SiteInfo::DetermineProcessLockURL`**: This function determines the process lock URL for a given `UrlInfo`. Errors here can lead to incorrect process locking, potentially allowing cross-site attacks.
-   **`SiteInfo::GetSiteForURLInternal`**: This function determines the site URL for a given `UrlInfo`. Incorrectly determining the site can lead to incorrect same-site/cross-site classifications.
-   **`NavigationRequest::GetUrlInfo`**: This function constructs a `UrlInfo` object based on the navigation request. Errors in populating the `UrlInfo` can lead to incorrect isolation decisions.
-   **`NavigationRequest::GetOriginToCommit`**: This function determines the origin that will be committed for the navigation. Incorrectly determining the origin can lead to security bypasses.
-   **`NavigationRequest::GetOriginForURLLoaderFactoryBeforeResponse` and `NavigationRequest::GetOriginForURLLoaderFactoryAfterResponse`**: These functions calculate the origin for creating a `URLLoaderFactory`. Errors here can lead to incorrect origin assignments for network requests, potentially allowing cross-origin leaks.

## Areas for Further Research: Potential Security Logic Issues

-   **`NavigationRequest::GetUrlInfo`** (`content/browser/renderer_host/navigation_request.cc`): This function is critical for security as it constructs the `UrlInfo` object that drives many isolation decisions. Potential areas for security logic issues include:
    -   Incorrectly parsing or interpreting the `Origin-Agent-Cluster` header, leading to incorrect `origin_isolation_request` values.
    -   Misinterpreting COOP headers, resulting in incorrect `is_coop_isolation_requested` values.
    -   Incorrectly identifying or handling prefetch navigations, leading to incorrect `is_prefetch_with_cross_site_contamination` values.
    -   Incorrectly overriding the origin for special URLs (e.g., `data:`, `about:blank`), potentially leading to security bypasses.
    -   Incorrectly handling sandbox flags or the `IsolateSandboxedIframes` feature, resulting in incorrect `is_sandboxed` or `unique_sandbox_id` values.
    -   Errors in determining the storage partition config, potentially leading to data leaks between partitions.
    -   Incorrectly parsing or interpreting COOP and COEP headers, leading to incorrect `web_exposed_isolation_info` values.
    -   Incorrectly identifying PDF navigations, potentially leading to incorrect isolation decisions for PDFs.
    -   Misinterpreting COOP headers, resulting in incorrect `common_coop_origin` values.
    -   Incorrectly parsing or interpreting the `Document-Isolation-Policy` header, leading to incorrect `cross_origin_isolation_key` values.
-   **`NavigationRequest::GetOriginToCommit`** (`content/browser/renderer_host/navigation_request.cc`): This function determines the origin that will be committed. Potential security issues include:
    -   Incorrectly handling error pages, potentially leading to incorrect origin assignments.
    -   Incorrectly determining the origin for `about:blank` navigations, potentially leading to security bypasses.
    -   Incorrectly handling `data:` URLs, potentially leading to incorrect origin assignments or leaks of precursor origins.
-   **`SiteInfo::Create`** (`content/browser/site_info.cc`): This function creates a `SiteInfo` object, which is crucial for determining process allocation. Potential security issues include:
    -   Incorrectly handling error pages, `data:` URLs, and `about:blank` navigations, potentially leading to incorrect process locking or site assignments.
    -   Misinterpreting the `Origin-Agent-Cluster` header, leading to incorrect decisions about origin-keyed processes.
    -   Incorrectly handling COOP headers, leading to incorrect decisions about dedicated processes.
    -   Incorrectly handling sandboxed frames, potentially leading to incorrect process assignments for sandboxed content.
-   **`BrowsingInstance::GetSiteInstanceForURL`** (`content/browser/browsing_instance.cc`): This function determines the `SiteInstance` for a given URL. Potential security issues include:
    -   Incorrectly finding or creating a `SiteInstance` for a given `UrlInfo`, potentially leading to incorrect process assignments.
    -   Incorrectly handling the default `SiteInstance`, potentially allowing sites to share a process when they should be isolated.
    -   Errors in managing the `creation_group`, potentially leading to inconsistent isolation decisions across tabs and windows.
-   **The role of `should_use_effective_urls`**: Incorrectly using this flag in `SiteInfo::GetSiteForURLInternal` can lead to incorrect site classifications, potentially allowing cross-site attacks.
-   **The purpose of the debug string in `GetOriginToCommitWithDebugInfo`**: While primarily for debugging, this string could potentially leak sensitive information in crash reports if not handled carefully.
-   **The relationship between `UrlInfo`, `SiteInfo`, and `NavigationRequest`**: Errors in the interaction between these components can lead to incorrect isolation decisions. For example, if `NavigationRequest` incorrectly populates `UrlInfo`, it can lead to an incorrect `SiteInfo` being created, resulting in incorrect process allocation.
-   **The usage of `With...` methods in `UrlInfoInit`**: Incorrect use of these methods can lead to misconfigured `UrlInfo` objects. For example, incorrectly setting `WithOriginIsolationRequest` can lead to incorrect origin isolation decisions.
-   **`SiteInfo::GetSiteForOrigin`** (`content/browser/site_info.cc`): This function determines the site for a given origin. Potential security issues include:
    -   Incorrectly extracting the registered domain, leading to incorrect site classifications.
    -   Incorrectly handling special origins (e.g., `localhost`), potentially leading to incorrect isolation decisions.

## Conclusion

Site isolation is a complex security mechanism that involves multiple components and careful coordination between different parts of the Chromium codebase. My current understanding is that it relies on the `UrlInfo` struct to package the URL with additional information, and uses `SiteInfo`, `SiteInstance`, and `BrowsingInstance` to enforce isolation policies. However, there are many potential areas for security logic issues that could compromise the effectiveness of site isolation. Further research and careful auditing of these areas are needed to ensure that site isolation provides robust protection against cross-site attacks.
