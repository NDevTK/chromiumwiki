# Chromium's CORS Implementation: A Security Deep Dive

## 1. Introduction

Cross-Origin Resource Sharing (CORS) is a fundamental security feature of the web platform that allows web pages to make requests to different origins than their own. Chromium's implementation is primarily located in the network service, under `services/network/cors/`. This document provides a security analysis of the key components responsible for enforcing CORS: `CorsURLLoaderFactory` and `CorsURLLoader`.

## 2. Architecture Overview

The CORS implementation in Chromium is designed as a layered system:

1.  **`CorsURLLoaderFactory`**: This factory is the primary entry point for all subresource requests originating from a renderer process. It acts as a gatekeeper, validating every request and deciding whether it needs to be subjected to CORS checks. It wraps the main `network::URLLoaderFactory`.

2.  **`CorsURLLoader`**: For requests that require CORS handling (i.e., cross-origin requests), the `CorsURLLoaderFactory` creates a `CorsURLLoader` instance. This loader manages the state of a single cross-origin request, including handling preflights and processing the final response according to CORS rules.

3.  **`PreflightController`**: This component, managed by the `NetworkContext`, handles the logic for sending and caching CORS-preflight `OPTIONS` requests. `CorsURLLoader` delegates preflight checks to this controller.

This layered approach ensures that all renderer-initiated requests are scrutinized and that the complex logic of the CORS protocol is encapsulated within dedicated components.

## 3. `CorsURLLoaderFactory`: The Gatekeeper

The `CorsURLLoaderFactory` is a critical security boundary. Its main job is to validate incoming requests from potentially malicious renderer processes.

### Key Security Responsibilities:

-   **Request Validation (`IsValidRequest`)**: This is the most security-critical method. It performs numerous checks to sanitize and validate the `ResourceRequest` object.
    -   **Initiator Lock Enforcement**: It verifies that the request's `request_initiator` matches the `request_initiator_origin_lock` of the factory. This is a vital defense to prevent a compromised renderer from making requests on behalf of other origins it does not control.
    -   **Forbidden Headers/Methods**: It blocks requests that use forbidden HTTP methods (e.g., `TRACE`) or contain unsafe headers.
    -   **Privilege Checks**: It ensures that untrusted renderers cannot use privileged `load_flags` or set `TrustedParams` that are reserved for the browser process.
-   **Decision Making**: Based on the validated request, it decides whether to create a `CorsURLLoader` (for CORS-enabled requests) or pass the request directly to the underlying network stack (if web security is disabled or the request doesn't require CORS).

### Attack Surface:

-   **Bypassing `IsValidRequest`**: Any bug in the validation logic could allow a compromised renderer to craft a malicious request that bypasses security checks.
-   **Initiator Lock Bypass**: A flaw in the initiator lock check could allow a renderer to impersonate another origin, effectively breaking the same-origin policy.

## 4. `CorsURLLoader`: The State Machine

Once a request is deemed to require CORS checks, a `CorsURLLoader` is created to manage its lifecycle.

### Key Security Responsibilities:

-   **Preflight Logic**: It determines if a preflight request is necessary by calling `NeedsPreflight`. This check is based on the request's method, headers, and credentials mode. If needed, it delegates to the `PreflightController`.
-   **Redirect Handling (`OnReceiveRedirect`)**: This is a highly security-sensitive area. When a redirect occurs, the `CorsURLLoader` re-evaluates the entire CORS policy for the new URL. It performs new preflight checks if necessary and updates its internal state, such as the `tainted_` flag, to ensure that information from one origin doesn't leak to another.
-   **Response Processing (`OnReceiveResponse`)**: When the final response arrives, the `CorsURLLoader` performs the final CORS access check based on the `Access-Control-Allow-Origin` header.
-   **Response Tainting**: Based on the outcome of the CORS checks, it sets the final response tainting (`kBasic`, `kCors`, or `kOpaque`). This is what ultimately prevents a renderer from reading the content of a cross-origin response it shouldn't have access to. An opaque response has its body completely blocked from the renderer.

### Attack Surface:

-   **State Machine Confusion**: A complex sequence of redirects or preflights could potentially confuse the `CorsURLLoader`'s state machine, leading to an incorrect security decision.
-   **Incorrect Tainting**: A bug in the tainting logic could result in an opaque response being incorrectly treated as a CORS-safe response, leading to information disclosure.
-   **Redirect Vulnerabilities**: Flaws in redirect handling could allow a request to be redirected to a malicious endpoint without proper security checks, or could leak information (like credentials) across origins.

## 5. Private Network Access (PNA)

The CORS mechanism is also leveraged to implement Private Network Access (formerly CORS-RFC1918).

-   When a public website attempts to make a request to a private IP address (e.g., `192.168.x.x`), the `CorsURLLoader` forces a CORS preflight.
-   This preflight includes a special header (`Access-Control-Request-Private-Network: true`).
-   The private network device must explicitly opt-in to receiving requests from the public internet by responding with `Access-Control-Allow-Private-Network: true`.
-   This mechanism prevents public websites from using a user's browser as a proxy to attack devices on their local network. The `CorsURLLoader` is central to enforcing this check.

## 6. Conclusion

Chromium's CORS implementation is a robust and multi-layered defense against cross-origin attacks.

-   The **`CorsURLLoaderFactory`** serves as a strong validation and filtering layer, protecting the network stack from malformed requests from renderers. Its initiator lock is a cornerstone of this defense.
-   The **`CorsURLLoader`** meticulously manages the state of individual requests, correctly handling complex scenarios like preflights and redirects, and ultimately ensuring that responses are tainted correctly to prevent information leaks.

The security of the web depends heavily on the correctness of this code. Any changes to these components, especially in the areas of request validation, state management, and redirect handling, must be scrutinized with extreme care.