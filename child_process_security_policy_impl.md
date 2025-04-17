# Component: ChildProcessSecurityPolicyImpl

## 1. Component Focus
*   **Functionality:** Central authority for granting and checking permissions for child processes (primarily renderers). Enforces process capabilities related to URL requests/commits, origin access, file access, WebUI bindings, etc. Works closely with `ProcessLock` to enforce Site Isolation.
*   **Key Logic:** Maintains security state (`SecurityState`) per child process. Provides `Grant*` methods to authorize access and `Can*` methods to check permissions at runtime.
*   **Core Files:**
    *   `content/browser/child_process_security_policy_impl.cc`
    *   `content/browser/child_process_security_policy_impl.h`

## 2. Core Concepts & Security Role

`ChildProcessSecurityPolicyImpl` is the gatekeeper for many security-critical operations performed by child processes. It ensures renderers adhere to the principle of least privilege based on the sites they are currently hosting (tracked via `ProcessLock`).

*   **Granting Permissions:** Methods like `GrantCommitURL`, `GrantRequestScheme`, `GrantReadFile` explicitly authorize a specific process (`child_id`) to access a resource. These grants are typically made during navigation or feature enablement based on higher-level browser logic.
*   **Checking Permissions:** Methods like `CanCommitURL`, `CanRequestURL`, `CanAccessDataForOrigin`, `CanReadFile` are called at critical moments (e.g., during navigation requests, commit validation, resource fetches, file operations) to verify if the requesting/committing process has the necessary grant.
*   **Site Isolation Enforcement:** The policy checks are often tied to the process's `ProcessLock`. For example, `CanCommitURL` checks if the URL is compatible with the sites the process is locked to. This prevents a process locked to site A from committing a URL for site B.
*   **`SecurityState`:** Internal class storing the permissions and lock state for each process.

## 3. Key Areas of Concern & VRP Relevance

Flaws in this class can lead to fundamental security boundary violations.

*   **Incorrect Permission Granting:** Granting overly broad permissions (e.g., `GrantCommitURL` for the wrong URL/origin, granting `GrantReadFile` inappropriately) can directly lead to sandbox escapes or information disclosure.
*   **Flawed Permission Checks:** Errors or omissions within the `Can*` methods can allow unauthorized actions.
    *   **Scheme Handling:** Incorrectly handling special schemes (`blob:`, `filesystem:`, `data:`, `about:`, `file:`, `javascript:`) in `CanCommitURL` or `CanRequestURL` can bypass intended restrictions. (Related: VRP2.txt#225 - Fenced frame file:// nav).
    *   **Origin Derivation:** Bugs in how origins are derived from URLs (especially complex ones like `blob:` or `filesystem:`) before checking permissions.
    *   **Process Lock Interaction:** Failing to correctly consult the `ProcessLock` state when checking URL permissions (`CanCommitURL`) could undermine Site Isolation.
*   **Interaction with Callers:** Many vulnerabilities arise when callers (like `RenderFrameHostImpl`) fail to use the policy checks correctly, or when the state checked by the policy is transient or incorrect at the time of the check.
    *   **Commit Validation Timing:** `RenderFrameHostImpl::ValidateDidCommitParams` relies on `CanCommitURL` (via `ContentBrowserClient::CanCommitURL` or directly) as a final check. If the process lock or granted permissions are incorrect *at that specific moment*, a bad commit might succeed. (Related: VRP `40059251` - popup origin confusion relies on checks happening before final commit state is known).
*   **Race Conditions/TOCTOU:** Potential for time-of-check/time-of-use issues if permissions are revoked between a check and the actual resource access (though less common for URL commits which are more transactional).

## 4. Key Functions Analysis

*   **`GrantCommitURL(child_id, url)` / `GrantCommitOrigin(child_id, origin)`:** Adds `url` or `origin` to the `SecurityState`'s `committed_origins_` set for the process. Called typically by navigation logic when a commit is expected.
*   **`GrantRequestScheme(child_id, scheme)`:** Allows requests for URLs with the given `scheme`. Used for schemes like `chrome-extension://`.
*   **`GrantReadFile(child_id, path)`:** Grants permission to read a specific file path.
*   **`CanCommitURL(child_id, url)`:**
    *   **Purpose:** Checks if the process `child_id` is allowed to commit `url`. This is a critical check for Site Isolation.
    *   **Logic:** Checks registered web-safe/pseudo schemes, handles special cases (`blob:`, `filesystem:`, `about:`, `data:`), checks against `SecurityState::committed_origins_`, and importantly, **validates against the process's `ProcessLock`** (via `SecurityState::CanCommitURL`).
    *   **Callers:** Crucially called during commit validation (`RenderFrameHostImpl::ValidateDidCommitParams` via `ContentBrowserClient`), also in `FileSystemURLLoaderFactory`, etc.
*   **`CanRequestURL(child_id, url)`:**
    *   **Purpose:** Checks if the process `child_id` is allowed to *request* `url`. Broader than `CanCommitURL`.
    *   **Logic:** Checks granted schemes (`SecurityState::CanRequestScheme`), handles special cases (e.g., `file:` depends on `SecurityState::can_read_all_files()`), checks `SecurityState::can_request_all_urls_`.
    *   **Callers:** Called during resource fetching logic, navigation initiation checks.
*   **`CanAccessDataForOrigin(child_id, url)`:** Checks if the process `child_id` can access data (e.g., cookies, storage) for the origin of `url`. Primarily relies on the process's `ProcessLock` and origin lock checks.
*   **`CanReadFile(child_id, path)` / `HasPermissionsForFile(child_id, path, permissions)`:** Checks if the process `child_id` has been explicitly granted read (or other) access to the specific `path`.

## 5. Areas Requiring Further Investigation

*   **`CanCommitURL` Logic:**
    *   Audit the handling of all special schemes (`blob:`, `filesystem:`, `about:`, `data:`, `javascript:`). Are origins derived correctly? Are checks consistent?
    *   Verify the interaction with `ProcessLock`. When `SecurityState::CanCommitURL` checks the lock, are there edge cases where the lock is incorrect or checked too early/late? (See `CanCommitURL` calling `GetProcessLock`).
    *   Analyze the known exemptions (e.g., related to `file:` URLs, `document.open` inheritance). Can these exemptions be exploited?
*   **`CanRequestURL` Logic:**
    *   Audit scheme checks (`SecurityState::CanRequestScheme`). Are there missing checks or bypasses?
    *   How does it handle redirects? Is the check performed correctly on each URL in a redirect chain?
*   **Granting Logic:**
    *   Trace callers of `GrantCommitURL`/`GrantCommitOrigin`. Are grants ever made based on incorrect or transient state during navigation?
    *   Trace callers of `GrantReadFile` and other file/filesystem grants. Is access granted too broadly or based on insufficient checks?
*   **`SecurityState` Management:** How is `SecurityState` updated during process creation, navigation, and process termination? Are there potential races or inconsistencies, especially regarding `ProcessLock` updates?
*   **Special URL Handling:** Systematically review how permissions are granted and checked for `blob:`, `filesystem:`, `data:`, `javascript:`, `about:blank`, `about:srcdoc` across all relevant `Grant*` and `Can*` methods.

## 6. Related VRP Reports
*   While direct VRPs against `ChildProcessSecurityPolicyImpl` are rare, its correct functioning is implicit in preventing many other bugs:
    *   Navigation bugs leading to commit in wrong process (Site Isolation bypasses).
    *   Origin confusion bugs (e.g., VRP `40059251`) where checks might pass based on transient opener state before commit validation uses the final state against `CanCommitURL`.
    *   URL scheme bypasses (e.g., VRP2.txt#225 Fenced Frame `file://` access likely required bypassing checks related to `CanCommitURL` or `CanRequestURL` for the resolved URL).
    *   Unauthorized file reads often imply a failure in granting/checking file permissions.

## 7. Cross-References
*   [site_instance.md](site_instance.md)
*   [process_lock.md](process_lock.md)
*   [site_isolation.md](site_isolation.md)
*   [navigation.md](navigation.md)
*   [render_frame_host_impl.md](render_frame_host_impl.md) (Specifically commit validation)
