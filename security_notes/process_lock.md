# Security Note: ProcessLock (`content/browser/process_lock.h`)

## 1. Summary

`ProcessLock` is a security-critical class that represents the security principal of a `RenderProcessHost`. It is the central data structure used to enforce Site Isolation, defining what a renderer process is allowed to do. A `ProcessLock` is not an active locking mechanism (like a mutex), but rather a descriptor of the security context that a process is "locked" to.

The `ChildProcessSecurityPolicyImpl` uses the `ProcessLock` of a process to make all of its critical access control decisions. An incorrect `ProcessLock` can lead to a complete breakdown of process-based security boundaries. This note is based on analysis of both the header and implementation files.

## 2. Core Concepts

*   **A Descriptor, Not an Active Lock**: `ProcessLock` is fundamentally a data-carrying class. It holds a `SiteInfo` object, which encapsulates all the security-relevant information about the principal the process is bound to.

*   **Three States**: A `ProcessLock` can be in one of three states:
    1.  **Invalid**: The initial state. The process is not yet associated with any `SiteInstance` and has no capabilities.
    2.  **AllowAnySite**: The process can host documents from any site that does *not* require a dedicated process. This is for shared, non-isolated processes. These locks are created with `ProcessLock::CreateAllowAnySite`.
    3.  **LockedToSite**: The process is strictly bound to a specific security principal (a site or origin) and cannot be used for any other. This is the cornerstone of Site Isolation. These locks are created with `ProcessLock::FromSiteInfo`.

*   **Immutable Security Context**: Once a process is locked to a site, that lock cannot be changed to a different site or be made less restrictive. `ChildProcessSecurityPolicyImpl::SetProcessLock` contains `CHECK`s to enforce this invariant, preventing a process from being dangerously repurposed.

*   **Derivation from `SiteInfo`**: The `ProcessLock` is almost entirely derived from a `SiteInfo`. The `SiteInfo` class contains the ground truth about a site's security properties, including its `AgentClusterKey` (which determines site vs. origin keying), sandbox flags, COOP/COEP isolation info, and more.

## 3. Security-Critical Logic & Implementation

*   **`ProcessLock::Create(isolation_context, url_info)`**: This is the primary static factory method. It creates a `SiteInfo` from the given `UrlInfo` and `IsolationContext`, and then uses that to construct the `ProcessLock`. The correctness of the `SiteInfo` creation logic is paramount, as any error there will result in an incorrect `ProcessLock`.

*   **Comparison Operators (`operator==`, `operator<`)**: These operators are security-critical because they are used to determine if a process's existing lock is compatible with a requested navigation. The implementation in `process_lock.cc` carefully compares all relevant fields of the underlying `SiteInfo` (except for `site_url`, which can differ for effective URLs) to ensure that two locks are only considered equal if their security contexts are truly identical.

*   **`IsLockedToSite()` and `AllowsAnySite()`**: These are the main predicates used by external code to query the state of the lock. Their implementation correctly checks the state of the internal `site_info_` member.

*   **`ToString()`**: While not directly involved in enforcement, this method is crucial for security analysis and debugging. It provides a detailed, human-readable string representation of the lock's state, including its site, sandbox flags, and isolation status, which is invaluable when analyzing crash reports related to security violations.

## 4. How It's Used in Enforcement

The `ProcessLock` itself does not enforce anything. It is a data object that is used by other, more active security components:

1.  **`RenderProcessHostImpl::SetProcessLock()`**: This method is called by a `SiteInstance` to apply the lock to its process.
2.  **`ChildProcessSecurityPolicyImpl::LockProcess()`**: This method stores the `ProcessLock` in its internal `SecurityState` map for the given `child_id`.
3.  **`ChildProcessSecurityPolicyImpl::CanAccess...` methods**: When a security check is needed (e.g., `CanAccessDataForOrigin`), the policy implementation retrieves the process's `ProcessLock` and compares it with the `ProcessLock` required for the requested resource. A mismatch results in a renderer kill.

The security of the entire process model relies on the `ProcessLock` accurately representing the intended security context and the `ChildProcessSecurityPolicyImpl` correctly enforcing it.

## 5. Related Files

*   `content/browser/child_process_security_policy_impl.h`: The primary consumer of `ProcessLock` objects. It uses them to make all access control decisions.
*   `content/browser/site_info.h`: The class that provides the underlying data for a `ProcessLock`.
*   `content/browser/renderer_host/render_process_host_impl.h`: The object that owns a renderer process and has a `ProcessLock` applied to it.
*   `content/browser/site_instance_impl.h`: The class that determines which `ProcessLock` a given navigation requires.