# SiteInstance (`content/browser/site_instance_impl.h`)

## 1. Summary

The `SiteInstance` is one of the most important security-critical classes in the browser process. It represents a "unit of privilege" for a collection of documents or workers. Its primary responsibility is to make process model decisions: for any given navigation, the `SiteInstance` determines which renderer process is allowed to host the content. It is the central component that makes **Site Isolation** possible.

A logical flaw in this class can lead to a complete breakdown of process-based security boundaries, allowing documents from different websites to share a process, which is a critical vulnerability.

## 2. Core Concepts

*   **The "Site" as a Security Principal:** The fundamental concept is the "site," which is a security principal typically defined by the URL's scheme and its effective Top-Level Domain plus one (eTLD+1). For example, `https://mail.google.com` and `https://docs.google.com` both belong to the `https://google.com` site. The `SiteInstance` uses a `SiteInfo` object to encapsulate this principal.

*   **Process Model Decisions:** The main job of a `SiteInstance` is to be the key for process allocation.
    *   When a new document needs to be rendered, the browser looks up or creates a `SiteInstance` for its destination URL.
    *   This `SiteInstance` is then used to find a suitable `RenderProcessHost`.
    *   If a process already exists for that `SiteInstance`'s site, it will be reused.
    *   If not, a new process will be created and will be **locked** to that `SiteInstance`'s site via the `ChildProcessSecurityPolicy`.

*   **Relationship to `BrowsingInstance`:**
    *   A `BrowsingInstance` is a collection of related browsing contexts (e.g., a tab and its popups) that can synchronously script each other.
    *   A `SiteInstance` lives *within* a `BrowsingInstance`. A single `BrowsingInstance` can contain multiple `SiteInstance`s, one for each distinct site that has been visited.
    *   The `SiteInstance` is what enforces the process-level separation *between* sites within the same `BrowsingInstance`.

*   **Effective URLs:** The class uses the concept of an "effective URL" (`GetEffectiveURL`). This is a security mechanism that allows the browser to treat certain URLs as if they belong to a different security origin. For example, a `chrome-native://` URL might have an effective URL of a privileged `chrome://` origin, ensuring it is always loaded in a highly-privileged process, not a web-content process.

## 3. Security-Critical Logic & Vulnerabilities

*   **Site Calculation:** The most critical logic is the calculation of the site from a URL (`SetSite`, `IsSameSiteWithURL`). A bug here is catastrophic. If two distinct websites (e.g., `https://example.com` and `https://evil.com`) were incorrectly calculated to be the same site, they would be placed in the same renderer process, completely defeating Site Isolation and allowing for universal cross-site scripting.

*   **Process Reuse and Selection:** The logic for deciding whether to reuse an existing process or create a new one is security-critical. A bug that causes a `SiteInstance` for `site-a.com` to incorrectly reuse a process that is locked to `site-b.com` would violate the process model. `RequiresDedicatedProcess()` and the internal process reuse policies are key here.

*   **The Default `SiteInstance`:** For sites that do not require a dedicated process (e.g., on platforms where Site Isolation is not fully enabled, or for `about:blank`), they are often grouped into a "default" `SiteInstance`. A bug that incorrectly classifies a site that *should* be isolated and places it in the default process would be a major security issue.

*   **State Management during Navigation:** The state of a `SiteInstance` (specifically, its `SiteInfo`) can be set lazily. During a complex navigation with redirects, it's critical that the final `SiteInfo` is determined correctly and that there's no state confusion that could lead to an incorrect process placement.

*   **Guest and Sandboxed Contexts:** The class has special handling for `<webview>` guests (`IsGuest()`) and sandboxed frames (`IsSandboxed()`). These contexts have different storage and permission models, and a bug that confuses a guest `SiteInstance` with a regular one could lead to a sandbox escape or information disclosure.

## 4. Key Functions

*   `Create...()`: The static methods for creating new `SiteInstance`s, which also create a new `BrowsingInstance`.
*   `GetRelatedSiteInstance(...)`: The primary method used during navigation to find or create a `SiteInstance` for a destination URL *within the same `BrowsingInstance`*.
*   `GetSiteInfo()`: Returns the security principal (`SiteInfo`) for this instance. This is the ground truth for security decisions.
*   `GetOrCreateProcess()`: The method that triggers the actual process selection or creation based on the `SiteInstance`'s state.
*   `IsSameSiteWithURL()`: The core comparison function that determines if a URL belongs to the same site as the `SiteInstance`.

## 5. Related Files

*   `content/browser/child_process_security_policy_impl.h`: The `SiteInstance` determines the policy (e.g., "this process is for `https://example.com`"), and the `ChildProcessSecurityPolicy` enforces it.
*   `content/browser/renderer_host/render_process_host_impl.h`: The class representing the actual OS process that a `SiteInstance` is assigned to.
*   `content/browser/browsing_instance.h`: The owner and manager of a collection of `SiteInstance`s.
*   `content/browser/site_info.h`: The data structure that encapsulates the "site" principal.