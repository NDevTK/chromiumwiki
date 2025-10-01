# BrowserInterfaceBroker (`content/browser/browser_interface_broker_impl.h`)

## 1. Summary

The `BrowserInterfaceBrokerImpl` is the central gatekeeper for all Mojo Inter-Process Communication (IPC) requests from a less-privileged execution context (like a sandboxed renderer frame or a service worker) to the privileged browser process. It implements the `blink::mojom::BrowserInterfaceBroker` interface, which is the pipe through which a renderer asks for access to powerful, browser-side capabilities.

This class is a cornerstone of Chromium's process security model. Its primary responsibility is to ensure that a given execution context can only connect to the specific Mojo interfaces it is explicitly authorized to access. A flaw in this class or its configuration could allow a compromised renderer to gain access to privileged interfaces, leading to a sandbox escape.

## 2. Core Concepts

*   **Static Binder Maps:** The core of the broker's security model is its use of `mojo::BinderMap`. A binder map is a dictionary that maps an interface name (a string) to a callback function that knows how to create and bind an implementation of that interface. The crucial security property is that these maps are populated **at compile time** based on the type of the execution context (`ExecutionContextHost`). This means a renderer cannot simply request an arbitrary interface by name; it can only request interfaces that have been explicitly registered for its context type in the browser's source code.

*   **Context-Specific Policies:** The class is templated on `ExecutionContextHost` (e.g., `RenderFrameHostImpl`, `ServiceWorkerHost`). This allows the `internal::PopulateBinderMap` functions to register different sets of binders for different types of contexts. For example, a `RenderFrameHost` has access to a wide range of interfaces related to rendering a page, while a `ServiceWorkerHost` has access to a different, more restricted set. This compile-time polymorphism is a key part of enforcing the principle of least privilege.

*   **Dynamic Capability Control via `MojoBinderPolicyApplier`:** While the *set* of available interfaces is static, the *timing* of when they can be bound can be controlled dynamically. The broker has an optional `MojoBinderPolicyApplier`. If this policy applier is installed (as it is for prerendering), it intercepts every `GetInterface` call and can decide to:
    *   **Grant:** Allow the bind to proceed immediately.
    *   **Defer:** Queue the bind request to be completed later (e.g., when a prerendered page is activated).
    *   **Cancel:** Reject the request entirely, which typically results in the renderer being terminated.
    This provides a powerful mechanism for enforcing temporary privilege reductions on contexts that are not fully active.

## 3. Security-Critical Logic & Vulnerabilities

*   **The Binder Registration (`browser_interface_binders.cc`):** The single most security-critical aspect of this system is not in this file, but in `content/browser/browser_interface_binders.cc`. That file is where the actual policies are defined. A developer incorrectly registering a highly-privileged interface for a context that shouldn't have it (e.g., exposing a filesystem API to an untrusted renderer frame) would create a direct sandbox escape vulnerability. The security of the entire system relies on the correctness of these static maps.

*   **Fail-Fast on Unknown Interface:** If a renderer requests an interface that is not in the binder map, `TryBind` fails, and the broker calls `host_->ReportNoBinderForInterface()`. This is a critical security reaction. It causes the offending renderer process to be terminated for making a "bad IPC." This prevents a compromised renderer from probing the browser process to discover what interfaces might be available.

*   **Policy Applier Logic:** The interaction with the `MojoBinderPolicyApplier` is a critical boundary. A bug that caused the broker to bypass the policy applier when one was set would completely defeat the security model for features like prerendering.

*   **Associated Interface Handling:** The policy applier treats "associated" interfaces differently (they cannot be deferred). A misclassification of an interface as non-associated when it is, in fact, associated could lead to IPC ordering bugs and potential deadlocks. Conversely, a failure to correctly apply a `kCancel` policy to a forbidden associated interface would be a critical failure.

## 4. Key Functions

*   `GetInterface(mojo::GenericPendingReceiver receiver)`: The main entry point for all incoming binder requests from the renderer. It is responsible for dispatching the request to the policy applier or directly to the binder map.
*   `ApplyMojoBinderPolicies(MojoBinderPolicyApplier* policy_applier)`: The method used by features like prerendering to install a dynamic security policy on the broker.
*   `BindInterface(mojo::GenericPendingReceiver receiver)`: The internal method that performs the lookup in the `binder_map_` and `binder_map_with_context_` and either executes the binder or reports an error.

## 5. Related Files

*   `content/browser/browser_interface_binders.cc` and `.h`: The most critical related files, as they contain the actual definitions of which interfaces are available to which execution contexts. **This is where the security policy is defined.**
*   `content/browser/mojo_binder_policy_applier.h`: The class that implements the dynamic policy layer for features like prerendering.
*   `content/browser/renderer_host/render_frame_host_impl.cc`: A primary example of an `ExecutionContextHost` that owns and uses a `BrowserInterfaceBrokerImpl`.
*   `third_party/blink/public/common/browser_interface_broker_proxy.h`: The corresponding class in the renderer process that is used to make `GetInterface` requests.