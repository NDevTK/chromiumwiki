# Service Worker Security Model

Service Workers are a powerful feature that allows websites to run background scripts for capabilities like offline support, push notifications, and network request proxying. This power necessitates a robust security model to prevent abuse. This document outlines the key architectural and security aspects of Service Workers in Chromium.

## Core Architecture

The Service Worker architecture is split between the browser process, which manages service workers, and the renderer process, where the service worker script actually executes.

-   **`ServiceWorkerContextCore`**: This browser-side class is the central hub for managing all service workers for a given storage partition. It owns the `ServiceWorkerRegistry`.
-   **`ServiceWorkerRegistry`**: Responsible for persistent storage of service worker registrations. It keeps track of which scopes are controlled by which service worker scripts, allowing them to persist across browser restarts.
-   **`ServiceWorkerRegistration`**: Represents a single registration for a given scope (e.g., `https://example.com/app/`). A registration can have multiple `ServiceWorkerVersion` objects associated with it (e.g., an `active` version and a `waiting` version).
-   **`ServiceWorkerVersion`**: Represents a specific version of a service worker's JavaScript file. It manages the lifecycle (installing, activating, running) of that script.
-   **`ServiceWorkerGlobalScope`**: The renderer-side global execution context for a service worker. It runs on a dedicated worker thread, separate from any document's main thread.

## Network Interception via `FetchEvent`

The primary power of a service worker comes from its ability to intercept network requests.

1.  **`ServiceWorkerMainResourceLoaderInterceptor`**: When a navigation occurs, this `URLLoaderInterceptor` is created in the browser process.
2.  **Controller Lookup**: The interceptor, via a `ServiceWorkerControlleeRequestHandler`, checks the `ServiceWorkerRegistry` to see if there is an active and controlling `ServiceWorkerRegistration` for the navigation's URL.
3.  **Dispatching the `FetchEvent`**: If a controlling service worker is found, the interceptor does not send the request to the network. Instead, it dispatches a `FetchEvent` to the service worker's thread in the renderer.
4.  **`respondWith()`**: The service worker's JavaScript code can listen for this `FetchEvent` and use the `event.respondWith()` method to provide a response. This response can be generated programmatically, retrieved from the Cache Storage API, or be a modified request that is then sent to the network.
5.  **Fallback**: If the service worker's `fetch` handler does not call `event.respondWith()`, the request falls back to the browser's default network stack.

## Security and Isolation Model

The Service Worker security model is designed to provide powerful capabilities without compromising the security of the web.

-   **Secure Contexts Only**: Service workers can only be registered and run on pages served from secure origins (HTTPS or localhost). This is a fundamental prerequisite that prevents man-in-the-middle attacks from installing malicious service workers.

-   **Origin-Bound**: A service worker is strictly bound to the origin of its script URL. It can only control pages and make requests within that same origin. It cannot intercept requests or access data from other origins.

-   **Isolated Global Scope**: The `ServiceWorkerGlobalScope` runs in a separate worker thread. It has **no direct access to the DOM** of the pages it controls. It cannot access the `window` or `document` objects, which prevents a compromised service worker from directly manipulating page content or stealing user information from the DOM. Communication with controlled pages is limited to the `postMessage` API.

-   **Script Caching and Updates**: The browser caches service worker scripts according to standard HTTP caching rules. The `update-via-cache` header provides some control over this. To prevent a vulnerable version of a script from being served from cache indefinitely, Chromium has a crucial mitigation: **it bypasses the HTTP cache for any service worker script that is more than 24 hours old**, forcing a revalidation request to the server. This ensures that developers can deploy security fixes that will be picked up by users within a reasonable timeframe.

-   **Limited API Surface**: The `ServiceWorkerGlobalScope` has access to a limited set of powerful APIs suitable for its purpose (e.g., `fetch`, `Cache`, `PushManager`), but it is denied access to many standard `Window` APIs that could pose a security risk, such as synchronous XHR.