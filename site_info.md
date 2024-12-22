# SiteInfo

This page details the `SiteInfo` class and its role in site isolation.

## Core Concepts

The `SiteInfo` class represents the site of a URL and determines if two URLs belong to the same site. Logic errors here can lead to incorrect same-site/cross-site classifications, potentially allowing cross-site attacks. The `SiteInfo` also stores the `AgentClusterKey` which represents the isolation requested through the use of `Document-Isolation-Policy`.

### Key Areas of Concern

-   Incorrectly classifying URLs as same-site or cross-site.
-   Incorrectly determining the process lock URL.
-   Incorrectly determining if a dedicated process is required.
-   Incorrectly determining if a process should be locked to a site.
-   Incorrectly determining if the process-per-site model should be used.
-   Incorrectly determining the `AgentClusterKey`.

### Related Files

-   `content/browser/site_info.cc`
-   `content/browser/site_info.h`

### Functions and Methods

-   `SiteInfo::Create`: Creates a `SiteInfo` object.
-   `SiteInfo::CreateInternal`: Creates a `SiteInfo` object, used internally by `SiteInfo::Create` and other methods.
    -   This method determines the process lock URL using `DetermineProcessLockURL` and the site URL using `GetSiteForURLInternal`.
    -   It also determines if the site requires a dedicated process, if JIT is disabled, and if V8 optimizations are disabled.
    -   It computes the `WebExposedIsolationLevel` based on the `WebExposedIsolationInfo` and `UrlInfo`.
    -   It handles special cases for error pages, sandboxed data URLs, and isolated origins.
-   `SiteInfo::CreateForTesting`: Creates a `SiteInfo` object for testing purposes.
-   `SiteInfo::CreateForErrorPage`: Creates a `SiteInfo` object for error pages.
-   `SiteInfo::CreateForDefaultSiteInstance`: Creates a `SiteInfo` object for default site instances.
-   `SiteInfo::CreateForGuest`: Creates a `SiteInfo` object for guest views.
-   `SiteInfo::DetermineProcessLockURL`: Determines the process lock URL for a given `UrlInfo`.
    -   This method uses `GetPossiblyOverriddenOriginFromUrl` to determine the correct origin to use for the process lock URL.
    -   For WebUI URLs of the form `chrome://foo.bar/`, the lock URL is based on the TLD (e.g., `chrome://bar/`).
-   `SiteInfo::GetSiteForURLInternal`: Determines the site URL for a given `UrlInfo`.
    -   This method uses `GetPossiblyOverriddenOriginFromUrl` to determine the correct origin to use for the site URL.
    -   For isolated sandboxed iframes in per-origin mode, the full origin is used as the site URL.
    -   For data URLs, the serialized opaque origin is used as the site URL.
    -   For blob URLs, the full URL with the GUID is used as the site URL.
    -   For isolated origins, the full origin is used as the site URL.
-   `SiteInfo::RequiresDedicatedProcess`: Determines if a dedicated process is required for a given site.
    -   This method checks for various conditions, including site-per-process mode, COOP isolation requests, isolated origins, sandboxed frames, error pages, PDF content, WebUI URLs, and embedder-specific requirements.
-   `SiteInfo::ShouldLockProcessToSite`: Determines if a process should be locked to a site.
    -   This method checks if the process should be locked to a site based on single-process mode, dedicated process requirements, WebUI URLs, and embedder-specific requirements.
-   `SiteInfo::ShouldUseProcessPerSite`: Determines if the process-per-site model should be used.
    -   This method checks if the process-per-site model should be used based on command-line switches, error pages, and embedder-specific requirements.
-   `SiteInfo::GetStoragePartitionConfigForUrl`: Returns the storage partition config for a given URL.
-   `SiteInfo::GetOriginBasedSiteURLForDataURL`: Returns a site URL for a data URL based on its origin.
-   `SiteInfo::ComputeWebExposedIsolationLevel`: Computes the web exposed isolation level based on the `WebExposedIsolationInfo` and `UrlInfo`.
-   `SiteInfo::ComputeWebExposedIsolationLevelForEmptySite`: Computes the web exposed isolation level for an empty site.
-   `SiteInfo::IsSamePrincipalWith`: Checks if two `SiteInfo` objects have the same security principal.
-   `SiteInfo::IsExactMatch`: Checks if two `SiteInfo` objects are an exact match.
-   `SiteInfo::ProcessLockCompareTo`: Compares two `SiteInfo` objects based on their process lock properties.
-   `SiteInfo::GetDebugString`: Returns a debug string representation of the `SiteInfo` object.
-   `SiteInfo::operator<<`: Overloads the output stream operator to print a `SiteInfo` object.
-   `SiteInfo::operator==`: Overloads the equality operator to compare two `SiteInfo` objects.
-   `SiteInfo::operator!=`: Overloads the inequality operator to compare two `SiteInfo` objects.
-   `SiteInfo::operator<`: Overloads the less-than operator to compare two `SiteInfo` objects.
-   `SiteInfo::CreateOnIOThread`: Similar to `Create`, but can only be called on the IO thread.
-   `SiteInfo::GetSiteForOrigin`: Returns the site of a given origin.
-   `SiteInfo::GetNonOriginKeyedEquivalentForMetrics`: Returns a new `SiteInfo` which is equivalent to the original, except that `is_origin_keyed` is false, and the remaining `SiteInfo` state is used to compute a new `SiteInfo` from a `UrlInfo` reconstructed from the original `SiteInfo`, minus any OAC opt-in request.
-   `SiteInfo::site_url()`: Returns the site URL associated with all of the documents and workers in this principal.
-   `SiteInfo::agent_cluster_key()`: Returns the `AgentClusterKey` of the execution contexts within this `SiteInfo`.
-   `SiteInfo::process_lock_url()`: Returns the URL which should be used in a `SetProcessLock` call for this `SiteInfo`'s process.
-   `SiteInfo::requires_origin_keyed_process()`: Returns whether this `SiteInfo` requires an origin-keyed process, such as for an `OriginAgentCluster` response header.
-   `SiteInfo::requires_origin_keyed_process_by_default()`: Indicates if the origin-keyed process is being used by default (e.g., via `kOriginKeyedProcessesByDefault`), rather than due to an opt-in OAC header.
-   `SiteInfo::is_sandboxed()`: Returns true when this `SiteInfo` is for an origin-restricted-sandboxed iframe.
-   `SiteInfo::unique_sandbox_id()`: Returns either `kInvalidUniqueSandboxId` or the unique sandbox id provided when this `SiteInfo` was created.
-   `SiteInfo::web_exposed_isolation_info()`: Returns the web-exposed isolation mode of the `BrowsingInstance` hosting `SiteInstances` with this `SiteInfo`.
-   `SiteInfo::web_exposed_isolation_level()`: Returns the web-exposed isolation capability of agents with this `SiteInfo`, ignoring the 'cross-origin-isolated' permissions policy.
-   `SiteInfo::is_guest()`: Returns true if this `SiteInfo` is for a `<webview>` guest.
-   `SiteInfo::is_error_page()`: Returns true if this `SiteInfo` is for an error page.
-   `SiteInfo::is_jit_disabled()`: Returns true if JIT is disabled for this `SiteInfo`.
-   `SiteInfo::are_v8_optimizations_disabled()`: Returns true if V8 optimizations are disabled for this `SiteInfo`.
-   `SiteInfo::is_pdf()`: Returns true if this `SiteInfo` is for PDF content.
-   `SiteInfo::is_fenced()`: Returns true if this `SiteInfo` is for content inside a fenced frame.
-   `SiteInfo::does_site_request_dedicated_process_for_coop()`: Returns true if there is a request to require a dedicated process for this `SiteInfo` due to a hint from the `Cross-Origin-Opener-Policy` header.
-   `SiteInfo::is_empty()`: Returns true if the `site_url()` is empty.

### Member Variables

-   `site_url_`: Stores the site URL.
-   `process_lock_url_`: Stores the process lock URL.
-   `requires_origin_keyed_process_`: Indicates if the site requires an origin-keyed process.
-   `requires_origin_keyed_process_by_default_`: Indicates if the site requires an origin-keyed process by default.
-   `is_sandboxed_`: Indicates if the site is sandboxed.
-   `unique_sandbox_id_`: Stores the unique sandbox ID.
-   `storage_partition_config_`: Stores the storage partition configuration.
-   `web_exposed_isolation_info_`: Stores the web exposed isolation information.
-   `web_exposed_isolation_level_`: Stores the web exposed isolation level.
-   `is_guest_`: Indicates if the site is a guest.
-   `does_site_request_dedicated_process_for_coop_`: Indicates if the site requests a dedicated process for COOP.
-   `is_jit_disabled_`: Indicates if JIT is disabled for the site.
-   `are_v8_optimizations_disabled_`: Indicates if V8 optimizations are disabled for the site.
-   `is_pdf_`: Indicates if the site is a PDF.
-   `is_fenced_`: Indicates if the site is a fenced frame.
-   `agent_cluster_key_`: Stores the agent cluster key.

### Further Investigation

-   The interaction between `GetPossiblyOverriddenOriginFromUrl` and different types of URLs, especially those with unique origins (e.g., blob URLs, filesystem URLs).
-   The logic for determining the process lock URL for WebUI URLs and how it interacts with the site URL.
-   The impact of incorrect origin handling on cross-origin communication and data access.
-   The security implications of using effective URLs in `SiteInfo::GetSiteForURLInternal`.
-   The logic for determining when a dedicated process is required and when a process can be reused.
-   The interaction between site isolation and other security mechanisms, such as Content Security Policy (CSP) and Cross-Origin Resource Sharing (CORS).
-   The logic within `SiteInfo::CreateInternal` to ensure that all fields are correctly initialized based on the provided `UrlInfo` and `IsolationContext`.
-   The usage of `SiteInfo::MakeSecurityPrincipalKey` to determine if two `SiteInfo` objects have the same security principal.
-   The logic within `SiteInfo::GetNonOriginKeyedEquivalentForMetrics` to create a non-origin-keyed equivalent for metrics purposes.
-   The logic within `SiteInfo::RequiresDedicatedProcess` to determine if a dedicated process is required for a given site.
-   The logic within `SiteInfo::ShouldLockProcessToSite` to determine if a process should be locked to a site.
-   The logic within `SiteInfo::ShouldUseProcessPerSite` to determine if the process-per-site model should be used.
-   The logic within `SiteInfo::ComputeWebExposedIsolationLevel` and `SiteInfo::ComputeWebExposedIsolationLevelForEmptySite` to ensure that the correct web exposed isolation level is computed.
-   The logic within `SiteInfo::IsSamePrincipalWith` to ensure that it correctly identifies `SiteInfo` objects with the same security principal.
-   The logic within `SiteInfo::IsExactMatch` to ensure that it correctly identifies `SiteInfo` objects that are an exact match.
-   The logic within `SiteInfo::ProcessLockCompareTo` to ensure that it correctly compares `SiteInfo` objects based on their process lock properties.
-   The interaction between the various member variables of `SiteInfo` and how they collectively determine the isolation characteristics of a site.
-   The handling of edge cases, such as when a site has multiple isolation requirements or when conflicting isolation requirements are encountered.

### Files Analyzed:

-   `chromiumwiki/README.md`
-   `content/browser/url_info.cc`
-   `chromiumwiki/url_info.md`
-   `content/browser/url_info.h`
-   `content/browser/site_info.cc`
-   `chromiumwiki/site_info.md`
-   `content/browser/site_info.h`
