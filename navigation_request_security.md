# NavigationRequest and Security

This page details the security aspects of the `NavigationRequest` class, including the application of various security policies and checks during the navigation process.

## Key Areas of Concern

-   Incorrect application of security policies, such as Content Security Policy (CSP), Cross-Origin Opener Policy (COOP), and Cross-Origin Embedder Policy (COEP).
-   Failure to properly validate the origin and other security-related parameters during redirects and cross-origin navigations.
-   Potential bypasses of security checks due to incorrect state management or handling of edge cases.
-   Vulnerabilities related to the interaction between `NavigationRequest` and other security mechanisms, such as sandboxing and process isolation.

## Security Policies and Checks

The `NavigationRequest` class is responsible for enforcing various security policies and performing security checks during the navigation process. These include:

-   **Content Security Policy (CSP)**: The `CheckCSPDirectives` and `CheckContentSecurityPolicy` methods are used to enforce CSP, including the `frame-src` and `fenced-frame-src` directives.
-   **Cross-Origin Opener Policy (COOP)**: The `ShouldRequestSiteIsolationForCOOP` method determines if a site should be isolated due to COOP. The `coop_status` member variable tracks the COOP status for the navigation.
-   **Cross-Origin Embedder Policy (COEP)**: The `ComputeCrossOriginEmbedderPolicy` and `CheckResponseAdherenceToCoep` methods are used to compute and enforce COEP. The `coep_reporter_` member variable is used to report COEP violations.
-   **Origin-Agent-Cluster Header**: The `AddOriginAgentClusterStateIfNecessary` and `DetermineOriginAgentClusterEndResult` methods handle the Origin-Agent-Cluster header and its impact on site isolation.
-   **Sandboxing**: The `sandbox_flags_initiator_` and `sandbox_flags_inherited_` member variables track the sandbox flags for the navigation.
-   **Private Network Requests**: The `private_network_request_policy_` member variable tracks the private network request policy for the navigation.
-   **Secure Contexts**: The `NavigationRequest` ensures that navigations to secure contexts are handled correctly and that appropriate security checks are performed.

## Origin and Site Isolation

The `NavigationRequest` plays a crucial role in determining the origin and site isolation for a navigation. The following methods are particularly important:

-   `GetOriginForURLLoaderFactoryBeforeResponse`: Determines the origin for the navigation before the response is received.
-   `GetOriginForURLLoaderFactoryAfterResponse`: Determines the origin for the navigation after the response is received.
-   `ComputeCrossOriginIsolationKey`: Computes the `CrossOriginIsolationKey` for the navigation.
-   `GetSiteInfoForURL`: Determines the `SiteInfo` for the navigation, which is used for process allocation decisions.

## Redirects and Cross-Origin Navigations

The `NavigationRequest` handles redirects and cross-origin navigations, ensuring that security policies are correctly applied and that sensitive information is not leaked across origins. The `OnRequestRedirected` method is called when a redirect occurs, and it updates the navigation state accordingly.

## Further Investigation

-   The detailed logic of the various security checks performed by `NavigationRequest`.
-   The interaction between `NavigationRequest` and other security-related classes, such as `ChildProcessSecurityPolicyImpl` and `ContentSecurityPolicy`.
-   The handling of edge cases and potential bypasses of security checks.
-   The impact of incorrect origin handling on cross-origin communication and data access.
-   The role of `NavigationRequest` in preventing cross-site scripting (XSS) and other web security vulnerabilities.

## Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_request_core.md`
-   `content/browser/renderer_host/navigation_request_lifecycle.md`
-   `content/browser/child_process_security_policy_impl.h`
-   `content/browser/child_process_security_policy_impl.cc`
