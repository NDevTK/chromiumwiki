# Browser-Initiated vs. Renderer-Initiated Navigations

In Chromium's navigation stack, a critical distinction is made between navigations initiated by the browser process and those initiated by a renderer process. This distinction is fundamental to the browser's security model, as renderer processes are untrusted and their requests must be treated with suspicion. The `browser_initiated` flag in `NavigationRequest` is the primary mechanism for tracking this distinction.

## Key Differences and Security Implications

### 1. Trust and Security Checks

-   **Renderer-Initiated Navigations are Less Trusted**: The fundamental difference is trust. The browser process is a trusted entity, while renderer processes are sandboxed and considered potentially malicious. Therefore, navigations originating from a renderer are subject to more stringent security checks.

-   **URL Request Permissions (`OnRequestRedirected`)**: For renderer-initiated navigations, the browser performs a `CanRequestURL` check via `ChildProcessSecurityPolicyImpl` on every redirect. This ensures that the renderer process (specifically, the `SiteInstance` that initiated the navigation) has the authority to navigate to the redirected URL. This check is skipped for browser-initiated navigations, as the browser is assumed to have the authority to navigate anywhere.

-   **Error Page Process (`ComputeErrorPageProcess`)**: When a renderer-initiated navigation is blocked (e.g., by a security policy), the resulting error page is intentionally committed in the *current* process, not the destination process. This is a crucial security measure to prevent a compromised renderer from gaining control over an error page in a privileged origin's process (e.g., a WebUI page). Browser-initiated navigations that fail will typically have their error pages committed in a process appropriate for the destination URL.

### 2. Exemption from Certain Security Hardening

-   **Self-Referential URL Check (`IsSelfReferentialURL`)**: Browser-initiated navigations are exempt from the check that prevents a frame from navigating to its own URL, a measure designed to stop infinite recursion bugs. This exemption is likely in place because browser-initiated navigations (like a user clicking a bookmark) are considered intentional and should not be blocked by this heuristic.

-   **`loadDataWithBaseURL` (`GetOriginForURLLoaderFactoryUnchecked`)**: The use of `loadDataWithBaseURL` is restricted to browser-initiated (or history) navigations. This prevents a renderer from being able to load arbitrary data into a frame and make it appear as if it came from a different, potentially privileged, origin.

### 3. Feature Behavior Differences

-   **User Activation**: The propagation of user activation is handled differently. For example, a browser-initiated navigation from a context menu is considered to have user activation. For renderer-initiated navigations, user activation is typically only propagated if the frame had a recent user gesture. This has security implications for APIs that require user activation, such as pop-ups or downloads.

-   **Navigation API (`navigate` event)**: The conditions under which the `navigate` event is dispatched for cross-document history navigations can differ based on whether the navigation was initiated by the browser or the renderer.

## Summary

The `browser_initiated` flag serves as a "trust bit" throughout the navigation process. Its state dictates the stringency of security checks and the availability of certain features.

-   **Browser-Initiated**: More trusted, fewer security checks. Assumed to be acting on behalf of the user.
-   **Renderer-Initiated**: Untrusted, more security checks. Assumed to be potentially malicious.

This clear separation of trust is a cornerstone of Chromium's security architecture. Any logic that blurs this distinction or incorrectly sets the `browser_initiated` flag could introduce serious security vulnerabilities. For example, if a renderer could somehow initiate a navigation and have it flagged as `browser_initiated`, it could bypass critical security checks. Therefore, the code paths that create `NavigationRequest` instances (`CreateBrowserInitiated` and `CreateRendererInitiated`) are extremely security-sensitive.