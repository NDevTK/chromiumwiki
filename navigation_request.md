# Component: NavigationRequest

## 1. Component Focus

*   **Class:** `content::NavigationRequest`
*   **Purpose:** Manages the entire lifecycle of a single navigation within a frame (`FrameTreeNode`). It determines critical parameters like the destination `UrlInfo`, selects the appropriate `RenderFrameHost`, enforces security policies (CSP, COOP, COEP, Origin Isolation, Process Locks), handles redirects, communicates with `NavigationThrottle`s and `CommitDeferringCondition`s, and ultimately coordinates the commit process with the chosen renderer process. It lives from the start of the navigation until it commits, is replaced, or is canceled.
*   **Key Files:**
    *   `content/browser/renderer_host/navigation_request.h`
    *   `content/browser/renderer_host/navigation_request.cc`

## 2. Potential Logic Flaws & VRP Relevance

*   **Incorrect `UrlInfo` Determination:** Errors in calculating or interpreting the `UrlInfo` (including origin, site, scheme) for the destination URL, especially with special schemes (`blob:`, `filesystem:`, `data:`, `about:srcdoc`, `javascript:`), redirects, or opaque origins, can lead to incorrect security decisions and isolation bypasses. (Related VRP data indicates issues with origin confusion post-redirect/crash VRP: 150, VRP2.txt: 173, 9502).
*   **Redirect Handling Errors:** Failure to correctly validate redirect targets (`CanRedirectToURL`), enforce security policies (COOP, COEP) during redirects (`OnRequestRedirected`), sanitize referrer information, or manage site instance selection across redirects can bypass security controls. (Origin spoofing via external protocol redirect - VRP: `40055515`).
*   **Security Policy Enforcement Failures:** Incorrect application or bypass of CSP (`CheckContentSecurityPolicy`), COOP (`ShouldRequestSiteIsolationForCOOP`), COEP (`ComputeCrossOriginEmbedderPolicy`, `CheckResponseAdherenceToCoep`), or Origin-Agent-Cluster policies during any stage of the navigation.
*   **Lifecycle State Management Errors:** Race conditions or incorrect state transitions (`SetState`) during the complex navigation lifecycle (including throttling, commit deferral, cancellation - `CANCELING`, `WILL_FAIL_REQUEST`) could lead to unexpected behavior or security bypasses.
*   **Process Selection/Commit Logic:** Errors in selecting the correct `RenderFrameHost` (`SelectFrameHostFor*` methods) or `SiteInstance`, or flaws in the commit process (`CommitNavigation`, `ReadyToCommitNavigation`, `CommitErrorPage`), potentially allowing navigation in an inappropriate process.
*   **Interaction with other Security Mechanisms:** Flaws in how `NavigationRequest` interacts with sandboxing, `ChildProcessSecurityPolicy`, `BrowsingInstance` swaps, or `NavigationHandle`.
*   **Handling of Special URLs/Cases:** Incorrect handling of `about:blank`, `about:srcdoc`, data URLs, downloads (`ComputeDownloadPolicy`), prerendering (`SetPrerenderActivationNavigationState`), or fenced frames (`is_embedder_initiated_fenced_frame_navigation_`) could lead to isolation bypasses or information leaks. (VRP2.txt: 1690 - `about:srcdoc` history leak).

## 3. Further Analysis and Potential Issues

*   **UrlInfo/SiteInfo/IsolationContext Logic:** Deep dive into how these structures are populated and used across different navigation types (browser-init, renderer-init, sync commit) and schemes. How are opaque origins consistently handled?
*   **Redirect Chain Security:** Analyze the complete handling of redirect chains, including multiple redirects, cross-origin redirects, and scheme changes. Ensure security policies are consistently applied at each step.
*   **Throttle and Deferral Interactions:** Examine the interaction between `NavigationThrottles` (which can cancel/defer) and `CommitDeferringConditions`. Can these interactions lead to unexpected states or bypasses?
*   **Error Handling Paths:** Audit the logic for handling navigation failures (`OnRequestFailedInternal`, `CommitErrorPage`, `CANCELING` state). Are error pages always committed in the correct process? Can error handling be manipulated?
*   **Origin Calculation:** Scrutinize `GetOriginForURLLoaderFactoryBeforeResponse` and `GetOriginForURLLoaderFactoryAfterResponse`. Are these always returning the correct origin, especially considering redirects and potential process swaps?
*   **Policy Container Management:** How is the `policy_container_builder_` managed throughout the navigation, especially across redirects and potential process changes?
*   **Interaction with NavigationHandle:** How does the state exposed via `NavigationHandle` align with the internal state of `NavigationRequest`? Can discrepancies be exploited?
*   **Fenced Frames & Prerendering:** How does `NavigationRequest` handle the specific security requirements and state management for fenced frames and prerender activations?

## 4. Code Analysis

### Creation
*   **Types:** Browser-initiated (`CreateBrowserInitiated`), Renderer-initiated (`CreateRendererInitiated`), Synchronous Renderer Commit (`CreateForSynchronousRendererCommit`). Distinguishing these is key for applying correct security checks.
*   **Parameters:** Takes numerous parameters (`CommonNavigationParams`, `BeginNavigationParams`, `CommitNavigationParams`, `FrameTreeNode`, initiator info, security policies, etc.) defining the navigation context. Validation of these parameters at creation is critical.

### Lifecycle & State Management
*   **States:** `NOT_STARTED`, `WAITING_FOR_RENDERER_RESPONSE`, `WILL_START_NAVIGATION`, `WILL_START_REQUEST`, `WILL_REDIRECT_REQUEST`, `WILL_PROCESS_RESPONSE`, `WILL_COMMIT_WITHOUT_URL_LOADER`, `READY_TO_COMMIT`, `DID_COMMIT`, `CANCELING`, `WILL_FAIL_REQUEST`, `DID_COMMIT_ERROR_PAGE`.
*   **Transitions:** Managed by `SetState`, validated via `DCHECK`. Triggered by events like receiving renderer response, throttle decisions, response processing, commit requests, errors.
*   **Throttling:** `NavigationThrottleRunner` executes `NavigationThrottles` at various stages (e.g., `WillStartRequest`, `WillRedirectRequest`). Throttles can `PROCEED`, `DEFER`, or `CANCEL`.
*   **Commit Deferral:** `CommitDeferringConditionRunner` manages `CommitDeferringConditions` that can delay commit (`ReadyToCommitNavigation`).
*   **Cancellation:** Can be triggered by throttles or errors. Enters `CANCELING` state, calls `OnRequestFailedInternal`.

### Security Checks & Policy Enforcement
*   **CSP:** `CheckContentSecurityPolicy`, `CheckCSPDirectives`. Checks `form-action`, `frame-src`, `fenced-frame-src` at multiple stages (start, redirect, response). Relies on `network::CSPContext::IsAllowedByCsp`.
*   **COOP:** `ShouldRequestSiteIsolationForCOOP`, `coop_status_`, `CrossOriginOpenerPolicyStatus::SanitizeResponse`. Determines if Site Isolation needed, tracks status, sanitizes response headers.
*   **COEP:** `ComputeCrossOriginEmbedderPolicy`, `CheckResponseAdherenceToCoep`, `coep_reporter_`. Computes policy, checks response adherence, reports violations.
*   **Origin-Agent-Cluster:** `AddOriginAgentClusterStateIfNecessary`, `DetermineOriginAgentClusterEndResult`.
*   **Redirect Security (`OnRequestRedirected`):**
    *   Validates target URL (`ChildProcessSecurityPolicyImpl::CanRedirectToURL`).
    *   Checks source access for renderer-initiated redirects (`CanRequestURL`).
    *   Enforces COOP/COEP.
    *   Updates parameters (`url`, `method`, `referrer`, `redirect_chain_`).
    *   Re-initializes cookie/device listeners.
    *   Re-computes `SiteInstance`.
*   **Origin Calculation for Network:** `GetOriginForURLLoaderFactoryBeforeResponse`, `GetOriginForURLLoaderFactoryAfterResponse`.
*   **Isolation Info:** `ComputeCrossOriginIsolationKey`, `ComputeWebExposedIsolationInfo`.
*   **Other Checks:** `CheckCredentialedSubresource`, `CheckAboutSrcDoc`.

### Process Management
*   **RenderFrameHost Selection:** `SelectFrameHostForOnResponseStarted`, `SelectFrameHostForOnRequestFailedInternal`. Determines the correct RFH to commit the navigation or error page.
*   **SiteInstance Selection:** Implicitly involved in `GetProcessHostForSiteInstance` calls made externally, based on `SiteInfo` and `IsolationContext` derived from `NavigationRequest` data.
*   **Process Lock:** Determines the required process lock based on `SiteInfo`.

### Key Function List (Selection)
*   `NavigationRequest::Create*`: Entry points for different navigation types.
*   `NavigationRequest::BeginNavigation`: Starts the process, initiates throttle checks.
*   `NavigationRequest::OnRequestRedirected`: Handles server redirects, performs security checks, updates state.
*   `NavigationRequest::OnResponseStarted`: Handles initial response processing, selects RFH, performs security checks.
*   `NavigationRequest::ReadyToCommitNavigation`: Signals readiness to commit, runs commit deferral conditions.
*   `NavigationRequest::CommitNavigation`: Coordinates the commit with the selected `RenderFrameHost`.
*   `NavigationRequest::OnRequestFailedInternal`: Central handler for navigation failures.
*   `NavigationRequest::CommitErrorPage`: Commits an error page in an appropriate process.
*   `NavigationRequest::CheckContentSecurityPolicy`: Enforces CSP.
*   `NavigationRequest::ComputeCrossOriginEmbedderPolicy`: Computes COEP.
*   `NavigationRequest::CheckResponseAdherenceToCoep`: Checks COEP adherence.
*   `NavigationRequest::ShouldRequestSiteIsolationForCOOP`: Checks if COOP requires isolation.

## 5. Areas Requiring Further Investigation
*   Detailed logic determining `UrlInfo`, `SiteInfo`, `IsolationContext`, especially for edge cases and special schemes.
*   Interaction between `NavigationThrottles` and `CommitDeferringConditions`, looking for race conditions or state bypasses.
*   Robustness of security policy enforcement (CSP, COOP, COEP, OAC) across redirects, process swaps, and error conditions.
*   Accuracy of origin calculation methods (`GetOriginForURLLoaderFactory*`) in all scenarios.
*   Correctness of error handling paths and error page process selection.
*   Security implications of interactions with `NavigationHandle`, `BrowsingInstance`, `ChildProcessSecurityPolicy`, sandboxing.
*   Handling of special navigations: `about:srcdoc`, data URLs, downloads, prerender, fenced frames.

## 6. Related VRP Reports
*   VRP: `40055515` (Origin spoof in external protocol dialogs via server-side redirect) - Relevant to redirect handling security.
*   VRP: `379652406` (Android address bar hidden after slow navigation) - Fixed issue related to navigation lifecycle/timing.
*   VRP: 150 (Top-level redirect from cross-origin iframe via same-site redirect/XSS) - Related to redirect security checks.
*   VRP2.txt#173 (Origin confusion for javascript/data URLs) - Relates to URL/scheme handling during navigation.
*   VRP2.txt#1690 (`about:srcdoc` session history leak) - Relates to handling special schemes/history.
*   VRP2.txt#9502 (URL spoof after crash) - Relates to navigation state after failure.

*(Note: This list focuses on reports most directly related to NavigationRequest logic. Issues in components like RPH, SiteInstance, or specific feature implementations might interact with NavigationRequest but are listed under their respective components unless the core flaw was in navigation handling itself.)*
