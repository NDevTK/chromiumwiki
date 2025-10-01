# ClientSecurityState (`services/network/public/mojom/client_security_state.mojom`)

## 1. Summary

`ClientSecurityState` is a critical MOJOM struct that encapsulates the security context of a client making a network request. Because Chromium has a multi-process architecture, the `NetworkService`, which runs in its own sandboxed process, often handles requests on behalf of less-trusted processes like renderers.

This struct is the primary mechanism by which the renderer process communicates its security posture to the `NetworkService`. The `NetworkService` then uses this information to make security-critical decisions about how to handle the request (e.g., whether to block it, modify it, or allow it to proceed). A compromised renderer could try to lie about its security state, so the `NetworkService` must treat this information as untrusted and validate it where possible.

## 2. Security-Critical Data Members

*   **`cross_origin_embedder_policy` (COEP):**
    *   **Purpose:** Communicates the document's [Cross-Origin Embedder Policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Cross-Origin-Embedder-Policy).
    *   **Security Impact:** COEP is a critical security feature that allows a document to isolate itself from non-cooperative cross-origin documents. It's a prerequisite for enabling high-resolution timers and `SharedArrayBuffer`, which can be used in speculative execution attacks like Spectre. The `NetworkService` uses this value to determine whether certain types of cross-origin responses should be blocked.

*   **`is_web_secure_context`:**
    *   **Purpose:** A boolean indicating whether the client document was loaded from a [Secure Context](https://developer.mozilla.org/en-US/docs/Web/Security/Secure_Contexts) (e.g., HTTPS, localhost).
    *   **Security Impact:** Many powerful web platform features (e.g., Service Workers, WebCrypto) are only available in secure contexts. The `NetworkService` may use this bit to enforce those restrictions, for example, by rejecting a service worker registration request initiated by an insecure context.

*   **`ip_address_space`:**
    *   **Purpose:** Specifies the IP address space of the client (e.g., `kPublic`, `kPrivate`, `kLocal`).
    *   **Security Impact:** This is the cornerstone of the **Private Network Access (PNA)** feature (formerly CORS-RFC1918). PNA prevents public websites from making requests directly to devices on a user's internal network (e.g., routers, printers, internal servers), which would be a classic Server-Side Request Forgery (SSRF) style attack. The `NetworkService` uses this to block requests from a less-private space (like `kPublic`) to a more-private one (like `kPrivate`).

*   **`private_network_request_policy`:**
    *   **Purpose:** Defines the enforcement mode for Private Network Access checks. It can be set to `kAllow`, `kWarn`, `kBlock`, or `kPreflight...`.
    *   **Security Impact:** This directly controls the PNA security feature. `kBlock` provides the strongest security by outright blocking forbidden requests. The `kPreflight...` modes require a special CORS preflight request to be sent and approved by the target private network device before the actual request is allowed, giving the target device an opportunity to opt-in.

## 3. Security Considerations & Attack Surface

*   **Untrusted Data:** The `NetworkService` must treat the `ClientSecurityState` received from a renderer as potentially malicious. A compromised renderer could lie about its state (e.g., claim to be in a secure context when it's not). The browser process provides a second, trusted `ClientSecurityState` in some cases (`trusted_params->client_security_state`) to mitigate this. The `SelectClientSecurityState` function is responsible for choosing the trusted version when available.

*   **Private Network Access (PNA) Bypass:** A bug in how the `NetworkService` interprets the `ip_address_space` or enforces the `private_network_request_policy` could neutralize the PNA protection, re-enabling SSRF-like attacks against users' internal networks.

*   **COEP Enforcement:** A flaw in enforcing COEP could lead to information leaks. For example, if a response that should be blocked by COEP is allowed through, it could be used in a speculative execution attack to read sensitive data from the renderer's memory space.

## 4. Related Files

*   `services/network/private_network_access_checker.cc`: The class in the `NetworkService` that contains the core logic for performing Private Network Access checks using the `ClientSecurityState`.
*   `services/network/url_loader.cc`: The `URLLoader` receives this struct and uses it to initialize security checks like PNA.
*   `services/network/cors/cors_url_loader.cc`: The CORS implementation also inspects this state to make decisions about cross-origin requests.
*   `content/browser/renderer_host/render_frame_host_impl.cc`: The class in the browser process that is responsible for creating and sending the `ClientSecurityState` to the `NetworkService` when a navigation or request occurs. This is where the "trusted" version of the state originates.