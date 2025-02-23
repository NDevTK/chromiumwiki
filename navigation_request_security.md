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
-   `content/browser/renderer_host/navigation_request_lifecycle.md`
-   `content/browser/child_process_security_policy_impl.h`
-   `content/browser/child_process_security_policy_impl.cc`
