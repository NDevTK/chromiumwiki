# ChildProcessSecurityPolicy (`content/browser/child_process_security_policy_impl.h`, `content/browser/child_process_security_policy_impl.cc`)

## 1. Summary

The `ChildProcessSecurityPolicyImpl` is the kernel of Chromium's browser security model. It is a singleton that lives in the privileged browser process and acts as the central authority for granting and checking the capabilities of all less-privileged child processes (e.g., renderers). Its fundamental purpose is to enforce the principle of least privilege, ensuring that a child process only has the bare minimum rights needed to perform its tasks.

This class is the primary enforcement point for **Site Isolation**. A bug in this class can easily lead to a complete breakdown of the browser's security boundaries. This note is based on analysis of both the header and implementation files.

## 2. Core Concepts

*   **Capability-Based Security:** Child processes start with almost no rights. The browser process must explicitly grant them capabilities on a case-by-case basis. This is managed by the internal `SecurityState` class, which holds all permissions for a given `child_id`.

*   **The Process Lock (`ProcessLock`):**
    *   The most important concept is the **`ProcessLock`**. When a renderer process is created, it is typically "locked" to a specific site (e.g., `https://example.com`) or marked as usable by any non-isolated site.
    *   `ChildProcessSecurityPolicyImpl` stores this lock on a per-process basis and uses it for all subsequent security decisions.
    *   The core enforcement logic is in `PerformJailAndCitadelChecks`, which compares the process's actual lock with the lock required for a requested origin. A mismatch results in a renderer kill.

*   **Origin vs. Site Granularity:** The policy is sophisticated enough to handle isolation at different granularities.
    *   **Site Isolation (Default):** Processes are locked to a "site" (scheme + eTLD+1).
    *   **Origin Isolation:** For sites that opt-in via the `Origin-Agent-Cluster` header or are configured via enterprise policy (`--isolate-origins`), the process lock is tightened to a full origin (scheme + host + port). The policy tracks these dynamic requests on a per-`BrowsingInstance` basis.

*   **State Lifetime Management (`Handle`):** The policy uses a `Handle` class to manage the lifetime of a process's security state. This is a reference-counting mechanism that ensures the `SecurityState` for a process outlives the `RenderProcessHost` itself, preventing race conditions where security checks could be made on a process that is already being torn down.

## 3. Security-Critical Logic & Responsibilities

This entire class is security-critical. A logic error in almost any public method could lead to a security boundary violation.

*   **Core Security Checks (`CanAccess...` methods):**
    *   **`CanCommitOriginAndUrl()`**: This is a critical gatekeeper for navigation. It verifies that a process is allowed to commit a document from a specific origin and URL. It performs multiple checks, including a call to `CanCommitURL`.
    *   **`CanCommitURL()`**: This method performs scheme-based checks (e.g., disallowing most pseudo-schemes) and, crucially, calls `CanAccessMaybeOpaqueOrigin`.
    *   **`CanAccessDataForOrigin()`**: This method checks if a process can access stored data (cookies, localStorage, etc.) for an origin. A bypass would lead to universal cross-site scripting (UXSS). This also funnels down to `CanAccessMaybeOpaqueOrigin`.
    *   **`CanAccessMaybeOpaqueOrigin()` / `PerformJailAndCitadelChecks()`**: This is the heart of the enforcement. It calculates the `ProcessLock` required for the requested URL/origin and compares it against the process's actual `ProcessLock`. It correctly handles opaque origins, sandboxed frames, and PDF documents. If the check fails, the renderer process is terminated. This function uses detailed crash keys (`LogCanAccessDataForOriginCrashKeys`) to aid in debugging violations found in the wild.

*   **Granting Capabilities:**
    *   `Grant...()` methods (e.g., `GrantReadFile`, `GrantCommitURL`, `GrantWebUIBindings`): These methods are the entry points for giving power to a child process. They are designed to be narrow. For example, `GrantRequestOfSpecificFile` grants access to a single file, not the entire `file://` scheme.
    *   A bug that grants an overly broad capability (e.g., granting WebUI bindings to a normal web renderer) would be a critical vulnerability, often leading to a full sandbox escape.

*   **Dynamic Isolation State:**
    *   The policy manages isolated origins that are not known at startup.
    *   `AddFutureIsolatedOrigins()`: Used for enterprise policies or features like `Origin-Agent-Cluster` that apply to all future browsing sessions.
    *   `AddCoopIsolatedOriginForBrowsingInstance()`: Used to enforce process isolation for a site that sent a Cross-Origin-Opener-Policy header, but only within the current `BrowsingInstance`.

*   **Enforcing Web Platform Security Features:**
    *   **Sandboxed Frames:** `IsAccessAllowedForSandboxedProcess()` enforces that sandboxed processes cannot access storage and can only host opaque origins.
    *   **PDFs:** `IsAccessAllowedForPdfProcess()` enforces that PDF-hosting processes cannot access storage, even for the origin of the PDF itself.
    *   **Blob/Filesystem URLs:** The policy correctly unwraps these URLs and performs checks on their inner origin.

## 4. Key Functions

*   `Add(int child_id, ...)` and `Remove(int child_id)`: Manages the lifecycle of a process's security state.
*   `LockProcess(...)`: Applies the `ProcessLock` to a process, defining its primary security boundary.
*   `CanCommitOriginAndUrl(...)`: The central gatekeeper for navigation.
*   `CanAccessDataForOrigin(...)`: The central gatekeeper for data access.
*   `PerformJailAndCitadelChecks()` (internal): The core enforcement logic that compares the process lock to the requested origin's requirements.
*   `Grant...()` / `Can...()` pairs: The mechanism for granting and checking specific capabilities.

## 5. Related Files

*   `content/browser/renderer_host/render_process_host_impl.cc`: The class that owns a renderer process and is the primary client of `ChildProcessSecurityPolicyImpl`.
*   `content/browser/site_instance_impl.cc`: Works closely with the security policy to determine process allocation and the correct `ProcessLock`.
*   `content/browser/process_lock.h`: Defines the `ProcessLock` class, which encapsulates the security principal a process is locked to.
*   `content/public/browser/site_isolation_policy.h`: Defines the higher-level policies that are implemented by `ChildProcessSecurityPolicyImpl`.