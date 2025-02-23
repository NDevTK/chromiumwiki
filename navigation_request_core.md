# NavigationRequest Core

This page details the core concepts of the `NavigationRequest` class and its related files.

## Core Concepts

The `NavigationRequest` class manages the navigation lifecycle and determines the `UrlInfo` for the navigation. Logic errors here can result in incorrect isolation decisions for navigations, potentially leading to security breaches.

### Key Areas of Concern

-   Incorrectly determining the `UrlInfo` for the navigation.
-   Errors in handling redirects and cross-origin navigations.
-   Incorrectly applying security policies during navigation.
-   Potential issues with the interaction between navigation and other security mechanisms.

### Related Files

-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_request.h`


### Files Analyzed:
* `content/browser/renderer_host/navigation_request.cc`

## Redirect Handling in `OnRequestRedirected`

The `OnRequestRedirected` method is a core part of `NavigationRequest`'s functionality, specifically designed to handle server redirects securely. It performs several crucial security checks and updates the navigation state during redirects:

-   **URL Validation**: It validates the redirect URL using `ChildProcessSecurityPolicyImpl::GetInstance()->CanRedirectToURL` to prevent unauthorized redirects, such as to `javascript:` URLs.
-   **Renderer-Initiated Navigation Check**: For renderer-initiated navigations, it verifies if the source has access to the redirected URL using `ChildProcessSecurityPolicyImpl::GetInstance()->CanRequestURL`.
-   **Policy Enforcement**: It enforces Cross-Origin Opener Policy (COOP) and Cross-Origin Embedder Policy (COEP) using `coop_status_.SanitizeResponse` and `EnforceCOEP` to control cross-origin interactions and prevent security vulnerabilities.
-   **Parameter Updates**: It updates navigation parameters, redirect chain, and navigation timings to maintain accurate state throughout the redirect process.
-   **Listener Re-initialization**: It re-initializes cookie and device bound session listeners to ensure proper tracking for the redirected URL.
-   **Site Instance Handling**: It computes and updates the `SiteInstance` for the redirect, which is essential for maintaining site isolation.

The method ensures that redirects are handled securely by validating URLs, enforcing security policies, and correctly managing navigation state and site isolation.
