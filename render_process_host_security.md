# RenderProcessHost Security

This page details the security aspects of the `RenderProcessHostImpl` class, including process locks, site isolation enforcement, and handling of bad messages.

## Process Locks

`RenderProcessHostImpl` uses process locks to enforce site isolation. A process lock restricts a renderer process to a specific site or origin, preventing it from accessing data from other sites or origins.

-   `SetProcessLock`: Sets the process lock for the `RenderProcessHost`.
-   `GetProcessLock`: Returns the process lock for the `RenderProcessHost`.
-   `IsProcessLockedToSiteForTesting`: Returns true if the process is locked to a site for testing.
-   `NotifyRendererOfLockedStateUpdate`: Notifies the renderer of a locked state update.

## Site Isolation Enforcement

`RenderProcessHostImpl` plays a key role in enforcing site isolation policies. It ensures that renderer processes are created with the appropriate process lock and that they are not reused for sites or origins that are not compatible with the lock.

-   `IsSuitableHost`: Checks if a `RenderProcessHost` is suitable for a given `SiteInstance`, considering factors such as process locks, WebUI bindings, and other security-related properties.
-   `MayReuseAndIsSuitable`: Checks if a `RenderProcessHost` may be reused and is suitable for a given `SiteInstance`.
-   `FilterURL`: Filters URLs to prevent the renderer process from accessing URLs it should not have access to.

## Handling of Bad Messages

`RenderProcessHostImpl` handles bad messages received from the renderer process. When a bad message is detected, the `ShutdownForBadMessage` method is called, which terminates the renderer process and optionally generates a crash report.

-   `OnBadMessageReceived`: Called when a bad message is received from the renderer process.
-   `ShutdownForBadMessage`: Shuts down the renderer process due to a bad message.

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
