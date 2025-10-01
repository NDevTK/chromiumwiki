# CORS Utility (`services/network/cors/cors_util.cc`)

## 1. Summary

This file contains utility functions that are fundamental to the enforcement of the Cross-Origin Resource Sharing (CORS) protocol. Its primary security-critical function, `CorsUnsafeNotForbiddenRequestHeaderNames`, is responsible for identifying which HTTP request headers are "unsafe" and therefore require a CORS preflight (an `OPTIONS` request) before the actual cross-origin request can be sent.

This check is a cornerstone of web security. It prevents web pages from arbitrarily sending sensitive headers on cross-origin requests, which could otherwise be used to probe internal networks or interact with legacy servers that might not properly validate `Origin` headers. A flaw in this logic could allow a malicious website to bypass the protections offered by CORS preflights.

## 2. Core Concepts

*   **CORS Safelist:** The Fetch standard defines a "CORS-safelisted request-header". These are headers that are considered "safe" enough to be sent on cross-origin requests without triggering a preflight. The list is small and includes headers like `Accept`, `Accept-Language`, `Content-Language`, and `Content-Type` (but only with specific, simple values like `application/x-www-form-urlencoded`).

*   **Forbidden Headers:** The Fetch standard also defines a list of "forbidden header names" (e.g., `Proxy-`, `Sec-`, `Host`) that can *never* be modified by JavaScript and are controlled exclusively by the user agent.

*   **Unsafe Headers & Preflight:** Any header that is neither forbidden nor on the safelist is considered "unsafe." If a cross-origin request includes any unsafe headers, the browser **must** send a CORS preflight request first. The server must then explicitly approve the unsafe headers via the `Access-Control-Allow-Headers` response header before the browser will send the actual request.

## 3. Security-Critical Logic & Vulnerabilities

The `CorsUnsafeNotForbiddenRequestHeaderNames` function implements the logic to identify unsafe headers.

*   **Iterating Headers:** The function iterates through all headers in a request. It first filters out any "forbidden" headers using `net::HttpUtil::IsSafeHeader`.

*   **The Safelist Check:**
    *   **Logic:** For the remaining headers, it calls `IsCorsSafelistedHeader`. This function (defined in `services/network/public/cpp/cors/cors.cc`) checks if the header's name and value conform to the safelist.
    *   **Vulnerability:** A bug in `IsCorsSafelistedHeader` that incorrectly classifies an unsafe header as safe would allow that header to be sent across origins without a preflight, breaking a core CORS security guarantee. For example, if a custom header like `X-CSRF-Token` were incorrectly safelisted, a malicious site could attempt to send it to a target that relies on its absence for security.

*   **The Safelist Value Size Limit:**
    *   **Logic:** This is a subtle but critical piece of security hardening. The function accumulates the total byte size of the *values* of all safelisted headers. If this total size exceeds a hardcoded limit (`kSafeListValueSizeMax = 1024`), it treats **all** the safelisted headers as if they were unsafe.
        ```cpp
        // services/network/cors/cors_util.cc:42
        if (safe_list_value_size > kSafeListValueSizeMax) {
          header_names.insert(header_names.end(), potentially_unsafe_names.begin(),
                              potentially_unsafe_names.end());
        }
        ```
    *   **Security Goal:** This mitigates the risk of an attacker trying to smuggle data or probe a server by packing a large amount of information into the values of otherwise "safe" headers. It acts as a defense-in-depth mechanism.
    *   **Vulnerability:** If this size check were removed or flawed, an attacker could potentially use safelisted headers to leak more information than intended across origins.

## 4. Key Functions

*   `CorsUnsafeNotForbiddenRequestHeaderNames(...)`: The primary function that identifies the list of headers requiring a preflight.
*   `IsCorsSafelistedHeader(...)` (in `services/network/public/cpp/cors/cors.cc`): The helper function that contains the actual safelist logic.
*   `net::HttpUtil::IsSafeHeader(...)`: The helper that checks against the list of forbidden headers.

## 5. Related Files

*   `services/network/cors/preflight_controller.cc`: This class is the primary consumer of `CorsUnsafeNotForbiddenRequestHeaderNames`. It uses the output to decide whether a preflight is necessary and to construct the `Access-Control-Request-Headers` header for the `OPTIONS` request.
*   `services/network/public/cpp/cors/cors.cc`: Contains the implementation of the core CORS safelist checks.
*   `net/http/http_util.cc`: Contains the implementation for checking for forbidden headers.
*   `services/network/cors/cors_url_loader.cc`: The main URL loader that orchestrates the entire CORS process, including invoking the preflight controller.