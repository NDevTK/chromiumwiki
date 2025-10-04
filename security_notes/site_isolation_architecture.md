# Security Architecture of Site Isolation in Chromium

## 1. Overview: The "Jail per Site" Model

Site Isolation is a fundamental security architecture in Chromium that changes the browser's process model to align with the web's security boundaries. Its primary goal is to mitigate the threat of a compromised renderer process being able to steal data from other websites.

Before Site Isolation, multiple websites could share the same renderer process. A single vulnerability in the rendering engine (e.g., in V8 or Blink) could allow a malicious site (`evil.com`) to read memory belonging to another site (`gmail.com`) that happened to be in the same process.

Site Isolation enforces that documents from different "sites" (e.g., `https://google.com`, `https://facebook.com`) are always placed in different renderer processes. This leverages the OS-level process isolation provided by the sandbox as a strong barrier against cross-site data theft. Even if an attacker compromises a renderer process, the sandbox prevents it from accessing the memory of other processes, effectively "jailing" the malicious code within the process for a single site.

## 2. Core Components and Concepts

### `SiteInstance`: The Unit of Privilege

The `SiteInstance` (`content/browser/site_instance_impl.cc`) is the most important security-critical class for implementing Site Isolation.

*   **Responsibility**: A `SiteInstance` represents a "unit of privilege." Its main job is to determine which renderer process is allowed to host a given piece of content.
*   **The "Site" Principal**: A `SiteInstance` is associated with a `SiteInfo` object, which represents a security principal, typically defined by the URL's scheme and its eTLD+1 (e.g., `https://google.com`).
*   **Process Allocation**: When a navigation occurs, the browser finds or creates a `SiteInstance` for the destination URL. This `SiteInstance` is then used as a key to look up an appropriate `RenderProcessHost`. If a process already exists for that site, it's reused; otherwise, a new one is created.
*   **Process Locking**: Once a process is chosen for a site that requires isolation, the `SiteInstance` **locks** the `RenderProcessHost` to that site's `SiteInfo`. This `ProcessLock` is a critical security guarantee that prevents that process from ever being used to host content from a different, isolated site. Attempting to do so results in a `NOTREACHED()` crash.

### `SiteIsolationPolicy`: The Decision Engine

The `SiteIsolationPolicy` (`components/site_isolation/site_isolation_policy.cc`) is the central policy engine that makes the high-level decisions about *when* and *how* to apply Site Isolation. It does not enforce the isolation itself but provides the configuration that the rest of the browser follows.

*   **Memory vs. Security Tradeoff**: The policy's most significant security function is balancing security with performance. On resource-constrained devices (primarily Android), `ShouldDisableSiteIsolationDueToMemoryThreshold()` will disable strict Site Isolation if the device has insufficient RAM. This is a deliberate product decision that weakens the security posture for users on lower-end devices.
*   **Heuristic-Based Isolation**: When full Site Isolation is disabled, the policy enables heuristic-based protections as a defense-in-depth measure:
    *   **Password Sites**: It can dynamically isolate a site *after* a user enters a password on it.
    *   **OAuth Sites**: It can dynamically isolate sites where a user logs in via OAuth.
    This is a reactive defense that prioritizes high-value targets.
*   **Web-exposed Controls**: `SiteIsolationPolicy` also respects headers like `Cross-Origin-Opener-Policy` (COOP), allowing security-conscious websites to explicitly request their own process.

## 3. The End-to-End Process Flow

1.  **Navigation Starts**: A navigation to `https://example.com` begins.
2.  **`SiteInstance` Selection**: The browser consults the current `BrowsingInstance` to find an appropriate `SiteInstance`.
    *   It calls `SiteInstance::GetRelatedSiteInstance()` to find or create a `SiteInstance` for the destination.
3.  **Isolation Decision**: The `SiteInstance` logic consults `SiteIsolationPolicy` to determine if `https://example.com` requires a dedicated process.
    *   This checks for global flags (`--site-per-process`), enterprise policies, dynamically isolated origins (from COOP or heuristics), etc.
4.  **Process Selection**: The `SiteInstance` calls `GetOrCreateProcess()`.
    *   If a suitable, unlocked process exists, it's reused.
    *   If a process locked to `https://example.com` exists, it's reused.
    *   If no suitable process exists, a new `RenderProcessHost` is created.
5.  **Process Lock**: The `SiteInstance` calls `LockProcessIfNeeded()` on the chosen `RenderProcessHost`, permanently binding it to the `https://example.com` site.
6.  **Content Rendering**: The content for `https://example.com` is sent to the locked renderer process to be displayed.

## 4. Security Posture and Conclusion

Site Isolation is a powerful and effective defense against a wide range of attacks, most notably cross-site data leakage and Spectre-style transient execution attacks.

*   **Strengths**:
    *   It leverages the OS-level process boundary, a very strong security primitive.
    *   The `ProcessLock` provides a hard guarantee against process reuse for incompatible sites.
    *   Heuristic-based isolation provides a valuable fallback for resource-constrained environments.

*   **Weaknesses / Areas of Concern**:
    *   **The Memory Tradeoff**: The fact that Site Isolation is disabled on low-memory devices is the single biggest limitation of the architecture. Users on these devices are less protected.
    *   **Complexity**: The interaction between global policies, enterprise policies, command-line flags, feature flags, and runtime heuristics is extremely complex. A bug in this configuration logic could lead to unintended security downgrades.
    *   **Browser Process Bugs**: Site Isolation protects against compromised renderers, but it does not protect the browser process itself. A vulnerability in the browser process or in the IPC layer that sandboxed processes use to communicate with it remains a critical threat.

In conclusion, Site Isolation fundamentally changes the browser's security model for the better, making it much harder for attackers to steal data. Its implementation is a complex interplay between the `SiteInstance` (the enforcer) and `SiteIsolationPolicy` (the decider), which together ensure that web content is confined to the appropriate process sandbox.