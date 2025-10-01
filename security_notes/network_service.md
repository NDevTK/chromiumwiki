# NetworkService (`services/network/network_service.h`)

## 1. Summary

The `NetworkService` is the root class of Chromium's network stack, running in its own dedicated, sandboxed process. It is responsible for managing all network activity for the entire browser, including all renderer processes and the browser process itself. It acts as a central authority for network-related state and policies, such as cookies, certificate verification, proxies, and the HTTP cache.

Its existence is a fundamental security boundary. By isolating all network-parsing code (which is notoriously complex and a common source of vulnerabilities) into a low-privilege, sandboxed process, a vulnerability in the network stack (e.g., a buffer overflow in an HTTP header parser) cannot directly compromise the main browser process or the underlying operating system.

## 2. Core Concepts

*   **Sandboxed Service:** The most critical aspect of the `NetworkService` is that it runs in a restrictive sandbox. The sandbox policy (`sandbox::policy::SandboxType::kNetwork`) blocks direct access to the filesystem (except for specific directories passed in by the browser), user interface, and most dangerous system calls. Its primary capability is making network connections.

*   **Owner of `NetworkContext`s:** The `NetworkService` owns and manages all `NetworkContext` objects. A `NetworkContext` represents a specific network session, typically corresponding to a single user profile (e.g., a main profile, an incognito profile). Each `NetworkContext` has its own isolated cookie jar, cache, and other session-specific state. This is the primary mechanism for enforcing data isolation between different browser profiles.

*   **Global Network State Management:** The `NetworkService` manages global state that is shared across all `NetworkContext`s. This includes:
    *   **Host Resolver (`HostResolverManager`):** The central DNS cache.
    *   **Certificate and Revocation Checking:** Manages trust stores and certificate transparency policies.
    *   **Proxy Settings:** Determines how to route network traffic.
    *   **First-Party Sets:** Manages the global list of First-Party Sets for cookie handling.

*   **Mojo Interface Hub:** It exposes the `mojom::NetworkService` interface, which is the main entry point for the browser process to configure the network stack. It also creates and manages the `mojom::NetworkContext` interfaces that are passed to renderer processes (via the browser) so they can make network requests.

## 3. Security-Critical Logic & Vulnerabilities

The security of the entire browser depends on the integrity of the `NetworkService` and its sandbox.

*   **Sandbox Escape:**
    *   **Risk:** The single greatest threat is a vulnerability within the network stack (e.g., in the URL parser, HTTP/2 implementation, or TLS library) that allows an attacker to achieve arbitrary code execution *inside* the Network Service's sandbox. From there, the attacker would try to find a second vulnerability in a Mojo IPC handler in the browser process to escape the sandbox and compromise the rest of the system.
    *   **Mitigation:** The sandbox policy for the Network Service is designed to be as restrictive as possible, minimizing the kernel attack surface available to a compromised process. All communication with more privileged processes (like the browser) must go through strictly defined Mojo interfaces.

*   **Cross-Context Data Leakage:**
    *   **Risk:** A logical flaw in the `NetworkService` that causes it to confuse `NetworkContext`s could lead to a catastrophic data leak. For example, if it incorrectly used the cookie store from a user's main profile to service a request from an incognito profile, it would violate the core privacy guarantee of incognito mode.
    *   **Mitigation:** The `CreateNetworkContext` method is a critical security boundary. It must correctly instantiate a new `NetworkContext` with its own isolated `URLRequestContext`, cookie store, and cache, based on the `NetworkContextParams` provided by the browser.

*   **Incorrect Policy Enforcement:**
    *   **Risk:** The browser process sends various security policies to the `NetworkService` for enforcement (e.g., `SetTrustTokenKeyCommitments`, `UpdateCtLogList`, `SetFirstPartySets`). If the `NetworkService` failed to apply these policies correctly or if they could be bypassed, critical security features would fail.
    *   **Mitigation:** The state for these policies is stored centrally within the `NetworkService` and passed down to each `NetworkContext` as it is created. The integrity of this state is critical.

*   **Raw Header Access:**
    *   **Risk:** The `SetRawHeadersAccess` method allows specific origins to bypass normal security checks and read raw, unfiltered response headers. This is a powerful and dangerous capability needed for extensions like DevTools. If this permission were granted to an untrusted web origin, it could be used to bypass security features like `HttpOnly` cookies.
    *   **Mitigation:** This permission is brokered by the browser process and is keyed by the renderer's `process_id`. The `NetworkService` must rigorously enforce that only renderers with the correct process ID can exercise this right for the origins they have been granted.

## 4. Key Functions

*   `CreateNetworkContext(...)`: The factory method for creating a new, isolated network session. This is a critical security boundary for data partitioning.
*   `SetParams(...)`: The main entry point for the browser process to configure global network policies.
*   `SetTrustTokenKeyCommitments(...)`, `SetFirstPartySets(...)`, etc.: Methods for updating global security policies.
*   `SetRawHeadersAccess(...)`: Grants a highly privileged capability to a specific process for a specific set of origins.

## 5. Related Files

*   `services/network/network_context.h`: The implementation of a single, isolated network session.
*   `content/browser/storage_partition_impl.cc`: The class in the browser process that owns a `NetworkContext` and is responsible for configuring it correctly.
*   `sandbox/policy/linux/bpf_network_policy_linux.cc`: An example of the `seccomp-bpf` sandbox policy applied to the Network Service on Linux, which defines its allowed kernel interface.
*   `net/url_request/url_request_context.h`: The core object within a `NetworkContext` that holds all the state for a request session (cookie store, cache, etc.).