# SiteInstance (`content/browser/site_instance_impl.h`, `content/browser/site_instance_impl.cc`)

## 1. Summary

The `SiteInstance` is one of the most important security-critical classes in the browser process. It represents a "unit of privilege" for a collection of documents or workers. Its primary responsibility is to make process model decisions: for any given navigation, the `SiteInstance` determines which renderer process is allowed to host the content. It is the central component that makes **Site Isolation** possible.

A logical flaw in this class can lead to a complete breakdown of process-based security boundaries, allowing documents from different websites to share a process, which is a critical vulnerability. This note is based on analysis of both the header and implementation files.

## 2. Core Concepts

*   **The "Site" as a Security Principal:** The fundamental concept is the "site," a security principal typically defined by the URL's scheme and its effective Top-Level Domain plus one (eTLD+1). For example, `https://mail.google.com` and `https://docs.google.com` both belong to the `https://google.com` site. The `SiteInstance` uses a `SiteInfo` object to encapsulate this principal.

*   **Process Model Decisions:** The main job of a `SiteInstance` is to be the key for process allocation.
    *   When a new document needs to be rendered, the browser looks up or creates a `SiteInstance` for its destination URL.
    *   This `SiteInstance` is then used to find a suitable `RenderProcessHost` via `GetOrCreateProcess()`.
    *   If a process already exists for that `SiteInstance`'s site (and it's a process-per-site model), it will be reused.
    *   If not, a new process will be created and will be **locked** to that `SiteInstance`'s site.

*   **Relationship to `BrowsingInstance`:**
    *   A `BrowsingInstance` is a collection of related browsing contexts (e.g., a tab and its popups) that can synchronously script each other.
    *   A `SiteInstance` lives *within* a `BrowsingInstance`. A single `BrowsingInstance` can contain multiple `SiteInstance`s, one for each distinct site that has been visited.
    *   The `SiteInstance` is what enforces the process-level separation *between* sites within the same `BrowsingInstance`.

*   **Effective URLs:** The class uses the concept of an "effective URL" (`GetEffectiveURL`). This is a security mechanism that allows the browser to treat certain URLs as if they belong to a different security origin. The logic is delegated to the embedder via `ContentBrowserClient::GetEffectiveURL`, meaning embedders like Chrome can have custom rules (e.g., for hosted apps).

## 3. Security-Critical Logic & Vulnerabilities

### Process Locking and Enforcement

*   **`LockProcessIfNeeded()`**: This is the primary enforcement mechanism. When a `SiteInstance` is assigned a process, this function sets a `ProcessLock` on the `RenderProcessHost`.
    *   The lock is created from the `SiteInfo` (`ProcessLock::FromSiteInfo`).
    *   It prevents the process from being used by other sites that require isolation.
    *   A `NOTREACHED()` crash will be triggered if code attempts to assign a site to a process that is already locked to an incompatible site, which is a critical defense-in-depth check.

*   **`ShouldUseProcessPerSite()` / `SiteInfo::RequiresDedicatedProcess()`**: These functions contain the core logic for deciding if a site *must* be isolated. A bug here could cause a sensitive site to be placed in a shared process. This decision considers:
    *   The global Site Isolation policy (`SiteIsolationPolicy::IsSiteIsolationEnabled`).
    *   Whether the URL's scheme is standard.
    *   Dynamically isolated origins (e.g., via `Origin-Agent-Cluster` or COOP).
    *   WebUI schemes.

### Site and Origin Comparison

*   **`IsSameSite()`**: This static method is the ground truth for comparing two URLs. A bug here is catastrophic. It correctly handles multiple complex cases:
    *   It respects `should_compare_effective_urls`.
    *   It checks for scheme equality.
    *   It uses `net::registry_controlled_domains::SameDomainOrHost` for the primary comparison.
    *   Crucially, it checks against the `ChildProcessSecurityPolicy` for dynamically isolated origins. If either URL matches an isolated origin, the comparison is promoted from site-level to origin-level.

### Special Contexts and Process Reuse

*   **The Default `SiteInstance`**: For sites that do not require a dedicated process, they are often grouped into a "default" `SiteInstance`.
    *   The logic in `CanBePlacedInDefaultSiteInstanceOrGroup()` determines what can go into this shared context. Notably, `file://` URLs are excluded to prevent the default process from accumulating dangerous file access grants.
    *   A bug that incorrectly classifies a site that *should* be isolated and places it in the default process would be a major security issue.

*   **Guest and Sandboxed Contexts**: The class has special handling for `<webview>` guests (`IsGuest()`) and sandboxed frames (`IsSandboxed()`). These contexts have different storage and permission models, and a bug that confuses a guest `SiteInstance` with a regular one could lead to a sandbox escape or information disclosure.

*   **Fenced Frames**: The static method `CreateForFencedFrame()` shows the special setup for this new context. A new `BrowsingInstance` is created to provide isolation, but crucially, the new `SiteInstance` may initially reuse the embedder's process to avoid jank before the fenced frame navigates. The actual process isolation occurs upon navigation within the fenced frame.

## 4. Key Functions

*   `Create...()`: The static methods for creating new `SiteInstance`s. `CreateForUrlInfo` is a key entry point.
*   `GetRelatedSiteInstance(...)`: The primary method used during navigation to find or create a `SiteInstance` for a destination URL *within the same `BrowsingInstance`*.
*   `GetOrCreateProcess()`: The method that triggers the actual process selection or creation based on the `SiteInstance`'s state and reuse policy.
*   `SetSite()` / `SetSiteInfoInternal()`: These methods assign the security principal (`SiteInfo`) to the `SiteInstance`, which is a critical, one-time operation.
*   `LockProcessIfNeeded()`: Applies the `ProcessLock` to the renderer process, enforcing the security boundary.

## 5. Related Files

*   `content/browser/child_process_security_policy_impl.h`: The `SiteInstance` determines the policy (e.g., "this process is for `https://example.com`"), and the `ChildProcessSecurityPolicy` enforces it and tracks dynamically isolated origins.
*   `content/browser/renderer_host/render_process_host_impl.h`: The class representing the actual OS process that a `SiteInstance` is assigned to. It holds the `ProcessLock`.
*   `content/browser/browsing_instance.h`: The owner and manager of a collection of `SiteInstance`s.
*   `content/browser/site_info.h`: The data structure that encapsulates the "site" principal.
*   `content/public/browser/content_browser_client.h`: Allows the embedder to override key security logic, such as effective URL resolution.