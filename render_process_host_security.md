# RenderProcessHost Security

This page details the security aspects of the `RenderProcessHostImpl` class, including process locks, site isolation enforcement, and handling of bad messages.

## Process Locks

`RenderProcessHostImpl` uses process locks to enforce site isolation. A process lock restricts a renderer process to a specific site or origin, preventing it from accessing data from other sites or origins.

-   `SetProcessLock`: Sets the process lock for the `RenderProcessHost`.
-   `GetProcessLock`: Returns the process lock for the `RenderProcessHost`.
-   `IsProcessLockedToSiteForTesting`: Returns true if the process is locked to a site for testing.
-   `NotifyRendererOfLockedStateUpdate`: Notifies the renderer of a locked state update.

### Process Lock Details

The `ProcessLock` class is central to process lock enforcement in Chromium. It represents a lock on a renderer process and encapsulates information about the site or origin that the process is restricted to.

There are different types of process locks:

*   **Site Lock:** Restricts a process to a specific site (e.g., `example.com`).
    *   Site Locks are the most common type of process lock and provide a strong level of site isolation.
    *   When a process is site-locked, it can only be used to render pages from the same site, as determined by the eTLD+1 of the URL.
    *   This prevents a compromised renderer process from accessing data from other sites, even if they share the same eTLD+1.
*   **Origin Lock:** Restricts a process to a specific origin (e.g., `https://example.com:443`).
    *   Origin Locks are more restrictive than Site Locks and provide even stronger isolation.
    *   A process with an Origin Lock can only be used for pages from the exact same origin, including the scheme, host, and port.
    *   Origin Locks are typically used for highly sensitive origins, such as those used by WebUI or extensions.
*   **Invalid Lock:** Represents a process that is not locked to any site or origin.
    *   Invalid Locks indicate that the process is not currently restricted to any particular site or origin.
    *   This type of lock is typically used for processes that are not yet hosting any content or are in the process of being shut down.
*   **Allows Any Site Lock:** Allows the process to be used for any site, but still enforces some security restrictions.
    *   Allows Any Site Locks are the least restrictive type of process lock and are used in specific situations where site isolation is not strictly required.
    *   Despite allowing any site, these locks still enforce some security restrictions, such as preventing access to certain privileged APIs.
    *   This type of lock is typically used for spare processes or processes hosting content that does not require strong isolation.

Process locks are enforced by the `RenderProcessHostImpl` in conjunction with the `ChildProcessSecurityPolicyImpl`. The `SetProcessLock` method in `RenderProcessHostImpl` updates the process lock in `ChildProcessSecurityPolicyImpl`.

The `IsSuitableHost` and `MayReuseAndIsSuitable` methods perform checks to determine if a `RenderProcessHost` is suitable for a given `SiteInstance` based on process locks and other security-related properties. These checks are crucial for maintaining site isolation by preventing the reuse of processes for incompatible sites or origins.

The `SiteProcessMap` class is used to track the mapping between sites and processes, aiding in process lock management and process reuse.

## Site Isolation Enforcement

`RenderProcessHostImpl` is the central class responsible for enforcing site isolation in Chromium. It works closely with `ChildProcessSecurityPolicyImpl` to ensure that renderer processes are properly isolated and cannot access resources from other sites.

**Key Methods for Site Isolation Enforcement:**

*   **`IsSuitableHost(RenderProcessHost* host, const IsolationContext& isolation_context, const SiteInfo& site_info)`:**
    *   This method checks if a given `RenderProcessHost` is suitable for hosting a `SiteInstance` with the provided `SiteInfo` and `IsolationContext`.
    *   It considers various factors, including process locks, WebUI bindings, and other security-related properties.
    *   The method ensures that the `RenderProcessHost` has the appropriate process lock for the `SiteInstance` and that they are compatible in terms of security policies.
*   **`MayReuseAndIsSuitable(RenderProcessHost* host, SiteInstanceImpl* site_instance)`:**
    *   This method extends `IsSuitableHost` by also checking if the `RenderProcessHost` may be reused for the given `SiteInstance`.
    *   It takes into account process reuse policies and resource thresholds to determine if reusing the process is appropriate.
    *   The method ensures that reusing the `RenderProcessHost` does not compromise site isolation or performance.
*   **`FilterURL(bool empty_allowed, GURL* url)`:**
    *   This method filters URLs to prevent the renderer process from accessing URLs it should not have access to.
    *   It checks the URL against the security policies enforced by `ChildProcessSecurityPolicyImpl` and returns a filtered URL or blocks the request if necessary.
    *   The method ensures that renderer processes only access authorized resources and prevents unauthorized URL access.

**`ChildProcessSecurityPolicyImpl` and Process Lock Enforcement:**

`ChildProcessSecurityPolicyImpl` is a crucial component in the site isolation enforcement mechanism. It maintains and enforces security policies for child processes, including renderer processes.

*   **Process Lock Management:** `ChildProcessSecurityPolicyImpl` manages process locks for renderer processes, associating each process with a specific lock type and site or origin.
*   **Security Policy Enforcement:** It enforces security policies by checking various conditions, such as URL access permissions, file system permissions, and other security-related constraints.
*   **Collaboration with `RenderProcessHostImpl`:** `ChildProcessSecurityPolicyImpl` works closely with `RenderProcessHostImpl` to ensure that renderer processes adhere to site isolation policies. `RenderProcessHostImpl` relies on `ChildProcessSecurityPolicyImpl` to perform security checks and enforce process locks.

By using process locks and collaborating with `ChildProcessSecurityPolicyImpl`, `RenderProcessHostImpl` effectively enforces site isolation, preventing compromised renderer processes from accessing sensitive data from other sites or origins.

## Handling of Bad Messages

`RenderProcessHostImpl` includes mechanisms to handle "bad" IPC messages from the renderer process. These are messages that violate expected formats or security rules, indicating potential issues like renderer bugs or security exploits.

**Bad Message Detection:**

Bad messages can be detected at various stages of IPC message processing, including:

*   **IPC Deserialization:** When the browser attempts to deserialize an incoming IPC message from the renderer, it validates the message format and structure. If deserialization fails due to unexpected data or format violations, it's considered a bad message.
*   **Message Handling Logic:** Even if a message is successfully deserialized, the browser-side handler might detect inconsistencies or violations of expected behavior during message processing. For example, a renderer sending a message with invalid arguments or violating API usage rules can be flagged as a bad message.

**`OnBadMessageReceived` Method:**

When a bad message is detected, the `RenderProcessHostImpl::OnBadMessageReceived` method is invoked. This method performs the following actions:

*   **Logs Error:** It logs an error message to the console, indicating the type of bad message received and the termination of the renderer process.
*   **Crash Reporting:** It triggers crash reporting mechanisms to collect debugging information and potentially identify the root cause of the bad message.
*   **Renderer Termination:** It calls the `ShutdownForBadMessage` method to terminate the compromised renderer process.

**`ShutdownForBadMessage` Method:**

The `ShutdownForBadMessage` method is responsible for gracefully shutting down the renderer process and handling crash reporting. It performs these steps:

*   **Process Termination:** It calls the `Shutdown` method to terminate the renderer process, preventing further communication or potential security breaches.
*   **Crash Dump Generation:** If specified by the `crash_report_mode` parameter, it triggers the generation of a crash dump file. This crash dump contains valuable debugging information that can be used to analyze the state of the renderer process at the time of the bad message and identify the cause of the issue.
*   **Crash Key Setting:** It sets crash keys to include relevant information about the renderer process, such as site isolation mode and process lock details. These crash keys help to categorize and analyze crashes related to bad messages and site isolation.

**Security Implications:**

Handling bad messages is crucial for maintaining the security and stability of Chromium. Bad messages can indicate various issues, including:

*   **Renderer Bugs:** Software defects in the renderer process might lead to the generation of malformed or invalid IPC messages.
*   **Security Exploits:** Malicious actors might attempt to exploit vulnerabilities in the renderer process by sending crafted bad messages to bypass security checks or gain unauthorized access.
*   **Memory Corruption:** Bad messages could be a symptom of memory corruption or other memory-related issues in the renderer process, potentially leading to crashes or unpredictable behavior.

By detecting and handling bad messages, `RenderProcessHostImpl` helps to mitigate these risks and ensure the overall security and reliability of the browser.

## Other Security-Related Aspects

-   `SetIsCrossOriginIsolated`: Sets whether the process is cross-origin isolated.
-   `SetIsIsolatedContext`: Sets whether the process is an isolated context.
-   `SetIsWebSecurityDisabled`: Sets whether web security is disabled.
-   `SetIsLockedToSite`: Sets whether the process is locked to a site.
-   `AddOriginAgentClusterStateIfNecessary`: Adds the origin to the list of origins that are isolated by the Origin-Agent-Cluster.
-   `DetermineOriginAgentClusterEndResult`: Determines the final result of the origin agent cluster isolation.
-   `CheckCSPEmbeddedEnforcement`: Checks if the Content Security Policy Embedded Enforcement is valid.
-   `CheckCredentialedSubresource`: Checks if the subresource request contains embedded credentials.
-   `CheckAboutSrcDoc`: Checks if the navigation is to an about:srcdoc URL.
-   `ShouldRequestSiteIsolationForCOOP`: Determines if a site should be isolated due to COOP.
-   `ComputeCrossOriginEmbedderPolicy`: Computes the Cross-Origin-Embedder-Policy.
-   `CheckResponseAdherenceToCoep`: Checks if the response adheres to the embedder's COEP.
-   `ComputeCrossOriginIsolationKey`: Computes the `CrossOriginIsolationKey` for the navigation.
-   `ComputeWebExposedIsolationInfo`: Computes the web-exposed isolation information.
-   `ComputeCommonCoopOrigin`: Computes the common COOP origin for the navigation.

## Further Investigation

-   The detailed logic of process lock enforcement and how it interacts with site isolation policies.
-   The mechanisms for detecting and handling bad messages from the renderer process.
-   The interaction between `RenderProcessHostImpl` and `ChildProcessSecurityPolicyImpl` in enforcing security policies.
-   The role of `RenderProcessHostImpl` in handling cross-origin navigations and redirects.
-   The security implications of process reuse and the criteria used to determine process suitability.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   `content/browser/child_process_security_policy_impl.h`
-   `content/browser/child_process_security_policy_impl.cc`
