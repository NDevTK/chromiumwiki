# Blink Resource Loading: `ResourceFetcher`

This document details the process of loading subresources within Blink, the rendering engine for Chromium. The central class for this process is `ResourceFetcher`, located in `third_party/blink/renderer/platform/loader/fetch/resource_fetcher.cc`. It acts as a gatekeeper and orchestrator for all subresource requests initiated by a document, such as images, scripts, and stylesheets.

## Overview of the Resource Loading Pipeline

When a document needs a subresource, it goes through the `ResourceFetcher`. The process can be broken down into the following key stages:

1.  **Request Initiation**: The process begins with a call to `ResourceFetcher::RequestResource`. This is the primary entry point for all subresource loads.
2.  **Preparation and Security Checks**: A `ResourcePrepareHelper` is used to prepare the request and perform initial security checks. This is a critical step where policies like CSP are enforced.
3.  **Cache and Preload Resolution**: The `ResourceFetcher` attempts to fulfill the request from various local sources before going to the network.
4.  **Network Request**: If the resource isn't available locally, a network request is initiated via a `ResourceLoader`.
5.  **Response Handling**: The `ResourceLoader` receives the response and passes the data to the `Resource` object, which decodes it and makes it available to the document.

## Detailed Analysis

### 1. `ResourceFetcher::RequestResource`

This is the main entry point. It orchestrates the entire loading process. Its first major action is to create a `ResourcePrepareHelper`.

### 2. `ResourcePrepareHelper::PrepareRequestForCacheAccess`

This helper class plays a crucial security role. Its primary job is to prepare the `ResourceRequest` object and run initial checks before any caching or network logic is invoked.

*   **CSP Enforcement**: This helper calls `Context().CheckAndEnforceCSPForRequest`. This is the **primary enforcement point for Content Security Policy**. It checks directives like `script-src`, `img-src`, `style-src`, etc., based on the resource type and the document's policy. If the request violates CSP, it is blocked immediately, and a `ResourceForBlockedRequest` is returned. This early check is highly efficient and secure as it prevents potentially harmful requests from ever reaching the cache or network.

### 3. Cache and Preload Resolution

After the initial security checks pass, `ResourceFetcher` tries to find the resource locally. The order of checks is important:

1.  **Static Data**: It first checks if the URL is a `data:` URI or if the resource is part of an MHTML archive. These are handled directly.
2.  **Preload Matching (`MatchPreload`)**: It then checks if the resource was preloaded (e.g., via `<link rel="preload">`). The matching logic here is strict:
    *   It checks not only the URL and resource type but also security-critical attributes like the request's CORS mode (`crossorigin` attribute), credentials mode, and integrity metadata.
    *   A mismatch (e.g., trying to use a `crossorigin="anonymous"` preload for a credentials-required request) will cause the preload to be rejected, and a warning is logged to the console. This prevents a less secure preloaded resource from being used in a more privileged context.
3.  **Memory Cache (`MemoryCache::Get()->ResourceForURL()`)**: If no preload matches, it consults the in-memory cache.

### 4. `DetermineRevalidationPolicy`

If a resource is found in the Memory Cache, this function decides how to use it. It's another important security checkpoint. It can decide to:

*   **`kUse`**: Use the cached version directly.
*   **`kRevalidate`**: Use the cached version but revalidate with the server using `If-None-Match` or `If-Modified-Since`.
*   **`kReload`**: Discard the cached version and fetch a new one. This happens for several security-sensitive reasons:
    *   **Type Mismatch**: The cached resource has a different type than the current request.
    *   **`Vary` Header Mismatch**: The request headers don't match the `Vary` header of the cached response.
    *   **`Cache-Control: no-store`**: The resource explicitly forbids storage.
    *   **Integrity Mismatch**: The new request has different `integrity` metadata than the cached resource.

### 5. `ResourceLoader` and the Network Stack

If a network request is necessary (`kLoad`, `kReload`, or `kRevalidate`), `ResourceFetcher` creates a `ResourceLoader`.

*   The `ResourceLoader` is the object that directly interacts with the network stack (via a `URLLoader` interface).
*   It is responsible for handling the asynchronous response.
*   **CORS**: `ResourceFetcher` itself doesn't perform the CORS checks. It sets the `mode` on the `ResourceRequest` (e.g., `cors`, `no-cors`). The actual CORS validation (checking `Access-Control-Allow-Origin`, etc.) is performed by lower-level loaders like `CorsURLLoader`, which wrap the `URLLoader` provided by the network service.

## Security Summary

*   **Upfront CSP Enforcement**: CSP is checked very early in the process by `ResourcePrepareHelper`, preventing blocked requests from proceeding.
*   **Strict Preload Matching**: The matching logic for preloaded resources is strict, preventing misuse across different security contexts.
*   **Secure Cache Revalidation**: `DetermineRevalidationPolicy` contains numerous checks to ensure that cached resources are only reused when it is safe to do so.
*   **Separation of Concerns**: `ResourceFetcher` orchestrates the high-level logic and security policy checks, while the `ResourceLoader` and the underlying network stack handle the specifics of network communication and protocol-level security like CORS.
*   **Context is King**: The `FetchContext` object is critical, as it provides the `ResourceFetcher` with the necessary security context (the document's origin, CSP, etc.) to make correct decisions.