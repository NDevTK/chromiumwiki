# MojoBinderPolicyApplier (`content/browser/mojo_binder_policy_applier.h`)

## 1. Summary

The `MojoBinderPolicyApplier` is a security-critical class that acts as a gatekeeper for Mojo interface requests originating from a renderer process. In Chromium's multi-process architecture, a sandboxed renderer process cannot directly access privileged resources; it must ask the browser process for them by requesting a connection (a "binder") to a specific Mojo interface. This class is the mechanism that enforces a security policy on those requests, deciding whether to grant, defer, or deny access.

Its primary use case is for speculative features like **Prerendering**. When a page is being prerendered in the background, it runs with significantly reduced privileges. It should not be able to access powerful APIs (like geolocation, device sensors, etc.) until the user actually navigates to it. `MojoBinderPolicyApplier` is the component that enforces this temporary privilege reduction.

## 2. Core Concepts

*   **Policy-Based Binding:** The class operates on a `MojoBinderPolicyMap`, which maps an interface name (e.g., `blink.mojom.GeolocationService`) to a policy (`kGrant`, `kDefer`, `kCancel`).

*   **State Machine (Modes):** The applier functions as a state machine with three modes:
    1.  `kEnforce`: The default mode. It strictly applies the policy from the map.
        *   `kGrant`: The binder is run immediately, connecting the renderer to the interface.
        *   `kDefer`: The binder request is stored in a queue (`deferred_binders_`). The connection is not established. This is the default for most interfaces during prerendering.
        *   `kCancel`: The binder request is dropped, and a `cancel_callback_` is run, often leading to the renderer process being terminated for making a disallowed request.
    2.  `kPrepareToGrantAll`: A transitional state used just before a prerendered page is activated. It stops canceling requests and instead defers everything that isn't explicitly `kGrant`.
    3.  `kGrantAll`: The final state, typically entered upon prerender activation. It immediately runs all deferred binders and approves all subsequent requests, effectively lifting the restrictions.

*   **Associated vs. Non-Associated Interfaces:** The class makes a critical distinction between two types of Mojo interfaces.
    *   **Non-Associated:** These are standard interfaces where messages can be reordered. Binder requests for these can be safely deferred.
    *   **Associated:** These interfaces are tied to a specific message pipe (like a `RenderFrameHost`) and their messages are strictly ordered. Deferring a binder for an associated interface is not possible, as it would block the entire pipe. Therefore, the policy for associated interfaces is binary: `kGrant` or `kCancel`.

## 3. Security-Critical Logic & Vulnerabilities

*   **The Policy Map:** The security of this mechanism rests entirely on the correctness of the `MojoBinderPolicyMap` it is given. An interface that is incorrectly mapped to `kGrant` when it should be `kDefer` or `kCancel` would create a "hole" in the prerender security model, allowing a prerendering page to access a powerful API it shouldn't have access to.

*   **State Transition Integrity:** The transition between modes is critical. A bug that causes `GrantAll()` to be called prematurely, or a failure to call it upon activation, would be a major flaw.
    *   **Premature `GrantAll()`:** A prerendering page would gain full privileges before the user has committed to navigating there, potentially allowing it to perform malicious actions in the background.
    *   **Failure to `GrantAll()`:** A legitimate page would fail to function correctly after activation because its deferred Mojo connections would never be established.

*   **Associated Interface Handling:** The binary `kGrant`/`kCancel` logic for associated interfaces is a critical security distinction. A bug that attempts to defer an associated interface binder could lead to deadlocks or other unexpected behavior, but more importantly, a failure to `kCancel` a forbidden associated interface would grant a sandboxed process access to a privileged, ordered message pipe it should not have.

*   **The `cancel_callback_`:** When a renderer requests a forbidden interface, the `cancel_callback_` is invoked. In the context of prerendering, this typically leads to the termination of the renderer process. This is a "fail-fast" security posture. A bug that prevents this callback from being run would allow a malicious renderer to probe for available interfaces without consequence.

## 4. Key Functions

*   `ApplyPolicyToNonAssociatedBinder(...)`: The main entry point for standard, deferrable interface requests.
*   `ApplyPolicyToAssociatedBinder(...)`: The entry point for associated interfaces, which only supports the grant/cancel logic.
*   `GrantAll()`: The method that transitions the state to `kGrantAll` and flushes the queue of deferred binders.
*   `CreateForSameOriginPrerendering(...)`, `CreateForPreview(...)`: Static factory methods that create an applier instance configured with a specific, curated policy map for a given security feature.

## 5. Related Files

*   `content/browser/browser_interface_broker_impl.cc`: The primary client of this class. It's the central broker that receives all `BindNewPipeAndPassReceiver` requests from renderers.
*   `content/browser/mojo_binder_policy_map_impl.cc`: The implementation of the policy map, where the actual interface-to-policy mappings are defined. This is where the security policy is actually encoded.
*   `content/browser/preloading/prerender/prerender_host.cc`: The class that manages the lifecycle of a prerendered page and is responsible for telling the `MojoBinderPolicyApplier` when to transition its state (e.g., by calling `GrantAll()` on activation).