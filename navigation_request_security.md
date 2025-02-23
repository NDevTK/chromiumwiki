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
    ### Content Security Policy (CSP) Enforcement Details

    The `NavigationRequest` class enforces Content Security Policy (CSP) at different stages of the navigation lifecycle using the `CheckContentSecurityPolicy` and `CheckCSPDirectives` methods.

    CSP checks are performed at the following stages:

    * **Before sending the request (`BeginNavigationImpl`):** The `CheckContentSecurityPolicy` method is called to perform initial CSP checks before the request is sent.
    * **On redirect (`OnRequestRedirected`):** The `CheckContentSecurityPolicy` method is called again to enforce CSP policies after a redirect.
    * **On response started (`BeginNavigationImpl` and `OnResponseStarted`):** The `CheckContentSecurityPolicy` method is called again after the response has started.

    The `CheckCSPDirectives` function performs the actual CSP directive checks. It takes several parameters, including:

    * `parent_context`, `parent_policies`: CSP context and policies of the parent frame (if any).
    * `initiator_context`, `initiator_policies`: CSP context and policies of the initiator frame (if any).
    * `has_followed_redirect`, `url_upgraded_after_redirect`: Flags indicating if a redirect occurred and if the URL was upgraded to HTTPS.
    * `is_response_check`: Flag indicating if the check is performed during response processing.
    * `disposition`: CSP check disposition (e.g., enforce or report-only).

    Within `CheckCSPDirectives`, individual CSP directives are checked using the `IsAllowedByCSPDirective` helper function. The directives checked include:

    * `form-action`
    * `frame-src`
    * `fenced-frame-src`

    The `IsAllowedByCSPDirective` function uses the `network::CSPContext::IsAllowedByCsp` method to determine if a given URL is allowed by a specific CSP directive, based on the provided CSP policies and context.

-   **Cross-Origin Opener Policy (COOP)**: The `ShouldRequestSiteIsolationForCOOP` method determines if a site should be isolated due to COOP. The `coop_status` member variable tracks the COOP status for the navigation.
    ### Cross-Origin Opener Policy (COOP) Enforcement Details

    The `NavigationRequest` class incorporates Cross-Origin Opener Policy (COOP) enforcement to isolate browsing contexts and mitigate security risks associated with cross-origin interactions. COOP is a crucial security policy that allows developers to control how a document can be accessed by other documents from different origins, protecting against attacks like cross-site scripting (XSS) and cross-site information leakage.

    **Key Methods and Variables:**

    *   `ShouldRequestSiteIsolationForCOOP()`: This method determines whether site isolation should be requested for a given navigation based on the COOP status. Site isolation is a security mechanism that isolates resources from different sites into separate processes, further enhancing security.
    *   `coop_status_`: This member variable of type `CrossOriginOpenerPolicyStatus` is used to track the COOP status throughout the navigation lifecycle. It stores information about the COOP policy and any violations encountered.
    *   `SanitizeResponse(network::mojom::URLResponseHead *response_head)`: This method, part of the `CrossOriginOpenerPolicyStatus` class, is called during the response processing stage to sanitize the response headers based on the COOP policy. It checks for any violations and updates the COOP status accordingly.
    *   `EnforceCOEP()`: This method enforces the Cross-Origin Embedder Policy (COEP), which is closely related to COOP and is used to further enhance cross-origin isolation.

    **Enforcement Process:**

    1.  **COOP Status Tracking:** The `coop_status_` member variable is initialized at the beginning of the navigation to track the COOP status.
    2.  **Response Sanitization:** During the response processing stage, the `SanitizeResponse()` method is called to check the response headers against the COOP policy. This method may detect violations and update the `coop_status_` accordingly.
    3.  **Site Isolation Request:** The `ShouldRequestSiteIsolationForCOOP()` method is used to determine if site isolation should be requested based on the COOP status.
    4.  **COEP Enforcement:** The `EnforceCOEP()` method is called to enforce the Cross-Origin Embedder Policy (COEP), which works in conjunction with COOP to provide robust cross-origin isolation.

    By enforcing COOP, `NavigationRequest` helps to ensure that browsing contexts are properly isolated, mitigating the risk of cross-origin attacks and protecting user data.

-   **Cross-Origin Embedder Policy (COEP)**: The `ComputeCrossOriginEmbedderPolicy` and `CheckResponseAdherenceToCoep` methods are used to compute and enforce COEP. The `coep_reporter_` member variable is used to report COEP violations.
    ### Cross-Origin Embedder Policy (COEP) Enforcement Details

    The `NavigationRequest` class enforces Cross-Origin Embedder Policy (COEP) to further strengthen cross-origin isolation and enable powerful features like SharedArrayBuffer. COEP is a security policy that allows developers to declare a document's cross-origin isolation state, ensuring that it is isolated from other origins to varying degrees.

    **Key Methods and Variables:**

    *   `ComputeCrossOriginEmbedderPolicy()`: This method computes the Cross-Origin Embedder Policy (COEP) for a given navigation request. It examines the relevant headers and determines the appropriate COEP value based on the server's policy.
    *   `CheckResponseAdherenceToCoep()`: This method checks whether the navigation response adheres to the computed COEP. It verifies if the response violates the COEP policy and returns a `BlockedByResponseReason` if a violation is detected.
    *   `coep_reporter_`: This member variable of type `CrossOriginEmbedderPolicyReporter` is used to report COEP violations. It allows Chromium to send reports to the server or other designated endpoints when COEP policies are violated.

    **Enforcement Process:**

    1.  **COEP Computation:** The `ComputeCrossOriginEmbedderPolicy()` method is called to determine the appropriate COEP value for the navigation based on response headers and other factors.
    2.  **Response Adherence Check:** The `CheckResponseAdherenceToCoep()` method is invoked to verify if the navigation response adheres to the computed COEP. This check ensures that the response satisfies the requirements of the COEP policy.
    3.  **Violation Reporting:** If a COEP violation is detected, the `coep_reporter_` is used to report the violation to the server or other configured endpoints. This reporting mechanism helps developers identify and address COEP-related issues.

    By enforcing COEP, `NavigationRequest` enhances the security of cross-origin interactions and enables the use of powerful features like SharedArrayBuffer, which require a secure, isolated environment.

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

## Analysis of `OnRequestRedirected` method

The `OnRequestRedirected` method in `navigation_request.cc` is crucial for handling security during redirects. Here's an analysis of its security aspects:

1.  **URL Validation and ChildProcessSecurityPolicy**:
    *   The method starts by checking if the browser client `ShouldOverrideUrlLoading` on Android. This is an embedder-specific security check.
    *   It then uses `ChildProcessSecurityPolicyImpl::GetInstance()->CanRedirectToURL` to verify if the renderer is allowed to redirect to the new URL. This is a crucial security check to prevent unauthorized redirects, especially to sensitive schemes like `javascript:`. If redirection is not allowed, the navigation fails with `net::ERR_ABORTED`. This is a positive security aspect as it prevents potential security issues arising from uncontrolled redirects.

2.  **Renderer-Initiated Navigation Check**:
    *   For renderer-initiated navigations, it additionally checks `ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL` to ensure the source has access to the redirected URL. This adds another layer of security, especially for renderer-initiated navigations, preventing potential unauthorized resource access.

3.  **COOP and COEP Enforcement**:
    *   The method calls `coop_status_.SanitizeResponse(response_head_.get())` and `EnforceCOEP()` to enforce Cross-Origin Opener Policy (COOP) and Cross-Origin Embedder Policy (COEP) respectively. These are important security policies to control cross-origin interactions and prevent information leaks. If these policies block the redirect, the navigation fails with the corresponding `network::mojom::BlockedByResponseReason`.

4.  **Navigation Timing and Parameters Update**:
    *   The method updates navigation timings and parameters, including `redirect_chain_`, `common_params_->url`, `common_params_->method`, and `common_params_->referrer`. It also updates `commit_params_->redirect_response` and `commit_params_->redirect_infos` to keep track of the redirect history. Sanitization of referrer is also performed using `Referrer::SanitizeForRequest`.

5.  **Cookie and Device Bound Session Listener Re-initialization**:
    *   On redirection, the `cookie_change_listener_` and `device_bound_session_observer_` are re-initialized if `ShouldAddCookieChangeListener` and `ShouldAddDeviceBoundSessionObserver` return true. This is important for tracking cookie changes and device-bound session expiry for the new URL after redirection, maintaining security and privacy.

6.  **Site Instance and Process Handling**:
    *   The method computes the `SiteInstance` to be used for the redirect and retrieves its `RenderProcessHost`. This is crucial for site isolation and process management, ensuring that redirects are handled within the correct security context.

**Potential Security Considerations and Further Investigation**:

*   **Error Handling for Javascript URLs**: The comment mentions that redirects to `javascript:` URLs should ideally display an error page with `net::ERR_UNSAFE_REDIRECT`. However, the current implementation ignores the navigation. It might be worth investigating if displaying an error page would be a more secure and user-friendly approach, rather than silently ignoring such redirects.
*   **Process Creation on Redirect Check**: The comment `TODO(crbug.com/388998723): The check may unintentionally create a process...` suggests a potential performance issue and possibly a security concern if process creation during security checks has unintended side effects. This could be further investigated and optimized.
*   **COOP/COEP Enforcement**: The method correctly enforces COOP and COEP during redirects. It's important to ensure that these policies are consistently and correctly applied throughout the navigation lifecycle, including redirects.
*   **Referrer Sanitization**: Referrer sanitization is performed, which is a good security practice to prevent leaking sensitive information in the Referer header.

**Overall Assessment**:

The `OnRequestRedirected` method seems to incorporate several important security checks and policy enforcements during redirect handling. It validates URLs, enforces CSP, COOP, and COEP, and handles origin and site isolation appropriately. However, the points mentioned under "Potential Security Considerations and Further Investigation" could be further explored to enhance the security and robustness of redirect handling in Chromium.

## BrowserURLHandlerImpl::RewriteURLIfNecessary and view-source: Scheme Analysis

The `BrowserURLHandlerImpl::RewriteURLIfNecessary` function in `content/browser/browser_url_handler_impl.cc` is responsible for rewriting URLs before navigation. It uses a chain of handlers to perform URL rewriting.

**Handler Order:**

The handlers are executed in the following order:

1.  `DebugURLHandler`: Handles renderer debug URLs.
2.  `HandleViewSource`: Handles `view-source:` URLs and applies a scheme whitelist.
3.  Custom handlers added by `ContentBrowserClient`.

This order ensures that debug URLs and `view-source:` URLs are handled before custom client-specific rewrites.

**`HandleViewSource` Scheme Whitelist:**

The `HandleViewSource` function uses a whitelist to allow `view-source:` for specific schemes: `http`, `https`, `chrome`, `file`, and `filesystem`. This whitelist prevents `view-source:` from being used to view active schemes like `javascript:` and `data:`, which is a good security practice.

**Security Considerations for `chrome-extension://`:**

The `chrome-extension://` scheme is not explicitly included in the default whitelist. Allowing `view-source:` for `chrome-extension://` could expose extension code, but extensions are generally considered trusted. It's important to consider the potential security implications of exposing extension code via `view-source:`.

**Further Investigation:**

-   **Scheme Whitelist Completeness**: Review the scheme whitelist in `HandleViewSource` and consider if `chrome-extension://` or other schemes should be added or removed.
-   **Custom Handler Security Risks**: Investigate potential security risks introduced by custom URL handlers added by content clients.
-   **`view-source:` Bypass Testing**: Investigate potential bypasses of `view-source:` restrictions and scheme whitelist.

## Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_request_core.md`
-   `chromiumwiki/navigation_request_core.md`
-   `chromiumwiki/navigation_request_lifecycle.md`
-   `chromiumwiki/child_process_security_policy_impl.md`
-   `chromiumwiki/child_process_security_policy_impl.cc`
