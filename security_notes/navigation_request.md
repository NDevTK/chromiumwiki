# NavigationRequest (`content/browser/renderer_host/navigation_request.cc`)

## 1. Summary

The `NavigationRequest` is an ephemeral object in the browser process that represents a single navigation from its initiation until it either commits or is cancelled. It is arguably one of the most complex and security-critical state machines in the entire browser. It is responsible for orchestrating the entire navigation process, including making the network request, running security checks, selecting an appropriate renderer process, and finally instructing the renderer to commit the new document.

A logical flaw in the `NavigationRequest` state machine can have catastrophic security consequences, including Same-Origin Policy violations, process model bypasses (breaking Site Isolation), URL spoofing, and the failure to enforce critical security headers like Content-Security-Policy (CSP) or Cross-Origin-Opener-Policy (COOP). This note is based on analysis of both the header and implementation files.

## 2. Core Concepts

*   **State Machine:** The navigation is modeled as a sequence of states (the `NavigationState` enum). The request progresses through states like `WILL_START_REQUEST`, `WILL_REDIRECT_REQUEST`, `WILL_PROCESS_RESPONSE`, and `READY_TO_COMMIT`. The integrity of this state machine is paramount; a bug that causes a state to be skipped could cause critical security checks to be missed.

*   **Navigation Throttles:** `NavigationRequest` uses a `NavigationThrottleRunner` to execute a series of `NavigationThrottle`s at key points in the navigation (e.g., before the request is sent, on redirect, when the response arrives). This is the primary mechanism for injecting security checks. Throttles can `PROCEED`, `CANCEL`, or `DEFER` the navigation, giving them complete control over its lifecycle.

*   **Process Selection:** A core responsibility of the `NavigationRequest` is to work with the `SiteInstance` and `RenderFrameHostManager` to determine the correct renderer process for the navigation. This involves complex logic to honor Site Isolation, Origin-Agent-Cluster isolation, and other process model policies.

*   **Policy Container Management:** The navigation is responsible for constructing the `PolicyContainer` for the destination document. This involves inheriting policies from the initiator, parsing new policies from HTTP response headers (CSP, COOP, COEP), and ensuring the final set of policies is sent to the renderer for enforcement at commit time.

## 3. Security-Critical Logic & Vulnerabilities

*   **State Machine Integrity:**
    *   **Risk:** If the state machine could be manipulated into an invalid transition (e.g., skipping `WILL_PROCESS_RESPONSE`), all security checks that rely on response headers (CSP, COOP, COEP, etc.) would be bypassed.
    *   **Mitigation:** The `SetState` method performs `DCHECK`s to validate all state transitions against a predefined state machine diagram (`base::StateTransitions`), ensuring the navigation proceeds through all required stages.

*   **URL and Origin Integrity:**
    *   **Risk:** The `NavigationRequest` manages multiple URLs throughout its lifetime (the initial URL, redirect URLs, the final URL). A bug that confuses these, especially using an earlier, trusted URL for a security check after a redirect to a malicious one, could lead to a security bypass.
    *   **Mitigation:** The class carefully maintains the `redirect_chain_` and updates its internal state at each step. Security checks are re-run at each stage of the navigation (e.g., `OnRedirectChecksComplete`). The `ValidateCommitOrigin` method performs a final, critical check before commit to ensure the origin being committed matches what is expected from the session history, preventing history-based attacks.

*   **Throttle Enforcement:**
    *   **Risk:** If a security-critical `NavigationThrottle` returns `CANCEL`, but the `NavigationRequest` fails to abort the navigation, a critical security check would be bypassed.
    *   **Mitigation:** The `OnNavigationEventProcessed` method is the central callback from the throttle runner. Its logic is responsible for correctly interpreting the throttle result and transitioning the `NavigationRequest` to the `CANCELING` or `WILL_FAIL_REQUEST` state, ultimately leading to the navigation being aborted via `OnRequestFailedInternal`.

*   **Process Model Enforcement:**
    *   **Risk:** A bug in the logic that determines the destination `SiteInstance` could place cross-site documents in the same process, breaking Site Isolation.
    *   **Mitigation:** The `NavigationRequest` collaborates with `RenderFrameHostManager`'s `GetFrameHostForNavigation` method, which contains the complex logic for process model decisions. The `NavigationRequest` provides all the necessary inputs, such as the `UrlInfo` and the initiator's context. A failure to provide the correct inputs could lead to a wrong decision.

*   **`ValidateCommitOrigin`:**
    *   **Risk:** Session history can be corrupted or manipulated. If a `NavigationEntry` for `bank.com` was somehow modified to point to `evil.com`, a history navigation could cause the browser to commit `evil.com` in the `bank.com` process.
    *   **Mitigation:** This function performs a final sanity check right before commit. It ensures that the origin being committed is compatible with the origin stored in the `FrameNavigationEntry`. A mismatch is a fatal error, preventing the commit.
        ```cpp
        // content/browser/renderer_host/navigation_request.cc:11880
        void NavigationRequest::ValidateCommitOrigin(
            const url::Origin& origin_to_commit) { ... }
        ```

*   **Security Header Parsing and Enforcement**:
    *   **Risk**: Incorrect parsing or application of headers like CSP, COOP, and COEP could lead to bypassing critical security policies.
    *   **Mitigation**: `OnResponseStarted` is the main entry point for this. It calls out to dedicated helpers like `ComputeCrossOriginEmbedderPolicy` and `CheckContentSecurityPolicy`. The resulting policies are stored in the `PolicyContainerHost` and sent to the renderer at commit time. The `CoopCoepSanityCheck` function also performs a last-minute verification that COOP+COEP pages are actually in an isolated process.

## 4. Key Functions

*   `BeginNavigation()`: Starts the navigation state machine.
*   `OnNavigationEventProcessed(...)`: The central callback from the `NavigationThrottleRunner`, which drives the state machine based on throttle results.
*   `OnResponseStarted(...)`: Called when response headers are received. This triggers `WILL_PROCESS_RESPONSE` throttles and the parsing of security headers like CSP and COOP.
*   `ReadyToCommitNavigation()`: A key transition point after all throttles and checks have passed, just before the `CommitNavigation` IPC is sent.
*   `CommitNavigation()`: Instructs the renderer process to commit the navigation and display the new document.

## 5. Related Files

*   `content/browser/renderer_host/navigator.cc`: The class that owns and manages the lifecycle of a `NavigationRequest`.
*   `content/browser/renderer_host/navigation_throttle.h`: The interface for security checks that can intercept the navigation. Many critical security features are implemented as throttles.
*   `content/browser/renderer_host/render_frame_host_manager.cc`: Works with `NavigationRequest` to make process model decisions.
*   `content/browser/renderer_host/policy_container_host.h`: The object, owned by the `NavigationRequest`, that accumulates the security policies for the destination document.
*   `content/browser/loader/navigation_url_loader.h`: The object that performs the actual network request on behalf of the `NavigationRequest`.