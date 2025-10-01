# ChildProcessSecurityPolicy (`content/browser/child_process_security_policy_impl.h`)

## 1. Summary

The `ChildProcessSecurityPolicyImpl` is the kernel of Chromium's browser security model. It is a singleton that lives in the privileged browser process and acts as the central authority for granting and checking the capabilities of all less-privileged child processes (e.g., renderers). Its fundamental purpose is to enforce the principle of least privilege, ensuring that a child process only has the bare minimum rights needed to perform its tasks.

This class is the primary enforcement point for **Site Isolation**, one of the most important security architectures in the browser. A bug in this class can easily lead to a complete breakdown of the browser's security boundaries.

## 2. Core Concepts

*   **Capability-Based Security:** Child processes start with almost no rights. The browser process must explicitly grant them capabilities on a case-by-case basis. This is in stark contrast to a model where a process starts with many rights that are later revoked.

*   **The Process Lock:**
    *   The most important concept is the **`ProcessLock`**. When a renderer process is created, it is typically "locked" to a specific site (e.g., `https://example.com`).
    *   `ChildProcessSecurityPolicyImpl` stores this lock and uses it for all subsequent security decisions.
    *   For example, if a process is locked to `site-a.com`, this policy will deny it the ability to commit a navigation to `site-b.com` or access `site-b.com`'s cookies. This is the core mechanism of Site Isolation.

*   **Origin vs. Site Granularity:** The policy is sophisticated enough to handle isolation at different granularities.
    *   **Site Isolation (Default):** Processes are locked to a "site" (scheme + eTLD+1).
    *   **Origin Isolation:** For sites that opt-in via the `Origin-Agent-Cluster` header or are configured via enterprise policy (`--isolate-origins`), the process lock is tightened to a full origin (scheme + host + port). This class contains the complex logic (`GetMatchingProcessIsolatedOrigin`) to determine the correct isolation scope for any given URL.

*   **Dynamic Permissions:** Permissions are not static. They are granted dynamically as the user navigates. For example, when the user opens a file, `GrantReadFile()` is called to give the renderer process the temporary right to access *only that specific file*.

## 3. Security-Critical Logic & Responsibilities

This entire class is security-critical. A logic error in almost any public method could lead to a security boundary violation.

*   **Commit and Access Checks:**
    *   `CanCommitOriginAndUrl()`: This is arguably the most critical method. It is called before a navigation commits and decides if a given process is allowed to render a document from a specific origin. A flaw here could allow two different sites to share a process, completely breaking Site Isolation.
    *   `CanAccessDataForOrigin()`: This method checks if a process can access stored data (cookies, localStorage, etc.) for an origin. It enforces that a process locked to `site-a.com` cannot read data from `site-b.com`. A bypass would lead to universal cross-site scripting (UXSS).

*   **Granting Capabilities:**
    *   `Grant...()` methods (e.g., `GrantReadFile`, `GrantCommitURL`, `GrantWebUIBindings`): These methods are the entry points for giving power to a child process. A bug that grants an overly broad capability (e.g., access to `/` instead of `/path/to/file.txt`, or granting WebUI bindings to a normal web renderer) would be a critical vulnerability, often leading to a full sandbox escape.

*   **State Management:** The policy maintains a `SecurityState` object for each child process. This state is constantly being updated as the user navigates. Race conditions or state confusion during process creation/destruction or complex navigation scenarios (e.g., redirects) are a major risk. The `Handle` inner class is an attempt to manage this by allowing the security state to outlive the `RenderProcessHost` it's associated with.

*   **Enforcing Web Platform Security Features:** This class is where many web platform security policies are actually enforced. For example, it checks if an origin is secure before granting access to powerful APIs and enforces restrictions related to `about:blank` and `data:` URLs.

## 4. Key Functions

*   `Add(int child_id, ...)` and `Remove(int child_id)`: Manages the lifecycle of a process's security state.
*   `LockProcess(...)`: Applies the `ProcessLock` to a process, defining its primary security boundary.
*   `CanCommitOriginAndUrl(...)`: The central gatekeeper for navigation.
*   `CanAccessDataForOrigin(...)`: The central gatekeeper for data access.
*   `Grant...()` / `Can...()` pairs: The mechanism for granting and checking specific capabilities.

## 5. Related Files

*   `content/browser/renderer_host/render_process_host_impl.cc`: The class that owns a renderer process and is the primary client of `ChildProcessSecurityPolicyImpl`. It calls `Add`, `Remove`, and the various `Grant` methods.
*   `content/browser/site_instance_impl.cc`: Represents a "site" and works closely with the security policy to determine process allocation.
*   `content/public/browser/site_isolation_policy.h`: Defines the higher-level policies that are implemented by `ChildProcessSecurityPolicyImpl`.