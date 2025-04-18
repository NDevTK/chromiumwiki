# Component: Service Workers

## 1. Component Focus
*   **Functionality:** Implements the Service Worker specification, allowing scripts to run in the background, intercept network requests (`FetchEvent`), manage caches, handle push notifications, and perform other tasks independent of a specific web page.
*   **Key Logic:** Lifecycle management (`ServiceWorkerVersion`), event dispatching (`ServiceWorkerVersion::DispatchFetchEvent`, `ServiceWorkerGlobalScope::DispatchFetchEventForMainResource`), request interception and response generation (`FetchEvent`, `FetchRespondWithObserver`), scope management, storage (`ServiceWorkerStorage`), client management (`ServiceWorkerContainerHost`).
*   **Core Files:**
    *   `content/browser/service_worker/` (Browser-side implementation, e.g., `service_worker_version.cc`, `service_worker_context_core.cc`, `service_worker_controllee_request_handler.cc`)
    *   `third_party/blink/renderer/modules/service_worker/` (Renderer-side implementation, e.g., `service_worker_global_scope.cc`, `fetch_event.cc`, `fetch_respond_with_observer.cc`)
    *   `third_party/blink/renderer/core/fetch/` (Core Blink Fetch API implementation, e.g., `fetch_manager.cc`)
    *   `third_party/blink/renderer/core/loader/` (Core Blink resource loading, e.g., `threadable_loader.cc`)
    *   `third_party/blink/public/mojom/service_worker/` (Mojo interfaces)

## 2. Potential Logic Flaws & VRP Relevance
*   **Request Interception Bypasses (FetchEvent):** Service workers intercepting network requests (`FetchEvent`) can modify or respond to requests, potentially bypassing security policies applied to the original request context. This is a major source of vulnerabilities.
    *   **VRP Pattern (CSP Bypass):** Intercepting requests and responding in a way that circumvents the controlled page's Content Security Policy (VRP: `598077`). See [content_security_policy.md](content_security_policy.md).
    *   **VRP Pattern (SameSite Cookie Bypass):** Handling a `FetchEvent` for a cross-site request and then re-issuing the fetch from the worker context, causing SameSite cookies (Lax/Strict) to be incorrectly sent (VRP: `1115438`, VRP2.txt#7521). See [privacy.md](privacy.md).
    *   **VRP Pattern (Mixed Content Bypass):** Potentially using service workers to facilitate loading of insecure content on secure pages (VRP2.txt#8497 - related to PWAs).
    *   **VRP Pattern (Origin Header):** Incorrectly setting the `Origin` header on requests made from within a `FetchEvent` handler (VRP2.txt#11875).
    *   **VRP Pattern (Extension SOP Bypass):** Extension service workers intercepting requests and potentially re-issuing them with elevated extension privileges, bypassing Same-Origin Policy for the original initiator.
        1. Browser (`ServiceWorkerVersion::DispatchFetchEvent`) sends original request details (`FetchAPIRequestPtr`) via Mojo to the renderer worker process.
        2. Renderer (`ServiceWorkerGlobalScope::DispatchFetchEventForMainResource`) receives the Mojo call.
        3. It creates a Blink `Request` object using the **worker's ScriptState** but populating it with data from the received `FetchAPIRequestPtr`.
        4. It creates a `FetchEvent` containing this `Request` object and dispatches it to the worker's JavaScript `onfetch` handler.
        5. The extension's `onfetch` handler calls `fetch()` (potentially on `event.request`).
        6. **This `fetch()` executes within the extension worker's context.** The critical question is how the core Blink fetch logic (e.g., `FetchManager`, `ThreadableLoader`) determines the security context (origin, credentials mode, initiator policy) for this worker-initiated fetch. VRPs suggest it improperly uses the **extension's context** instead of the original intercepted request's context.
        (Related to VRP2.txt#1234, #8913). See [extension_security.md](extension_security.md).
*   **Lifecycle Management Issues:** Race conditions or state inconsistencies during registration, update, activation, or termination.
*   **Cache Manipulation/Poisoning:**
    *   **VRP Pattern (Cache Side-Channel):** Leaking information via cache timing or state (VRP2.txt#14773).
*   **Origin/Scope Confusion:** Errors enforcing scope (`ServiceWorkerContextCore::FindRegistrationForScope`) or validating origins during registration or client matching.
*   **Information Leaks:**
    *   **VRP Pattern (Pixel Data Leak):** Interacting with Canvas APIs (VRP2.txt#7318, #7346).
*   **Interaction with Other Features:** Exploits arising from interactions with other browser features.
    *   **VRP Pattern (Payment Request XSS):** (VRP2.txt#276). See [payments.md](payments.md).
    *   **VRP Pattern (Extension Context):** General isolation bypasses when running in extension context (VRP2.txt#785).

## 3. Further Analysis and Potential Issues
*   **FetchEvent Security Context (Renderer):** Deep dive into the `fetch()` call path initiated from `ServiceWorkerGlobalScope`. How does the core Blink fetch logic (`core/fetch/FetchManager.cc`, `core/loader/ThreadableLoader.cc`, etc.) determine and apply security context parameters (credentials mode, CORS mode, referrer policy, initiator origin, CSP) when the fetch is initiated by a worker handling an `onfetch` event? **Critically, verify if it correctly uses the context of the *intercepted* request or incorrectly inherits from the worker's context (especially problematic for extension workers).** (Relates to VRP: `1115438`, VRP2.txt#1234, #8913, #11875).
*   **Lifecycle Races:** Analyze state transitions in `ServiceWorkerVersion` (e.g., `SetStatus`, activation, redundancy).
*   **Scope Enforcement:** Audit scope matching logic (`ServiceWorkerContextCore::FindRegistrationForScope`, `NavigationLoaderInterceptor::FindRegistrationForUrl`).
*   **Storage Security:** Review script storage (`ServiceWorkerStorage`).
*   **Cross-Origin Communication:** Analyze `ServiceWorkerGlobalScope::postMessage`.
*   **Update Process:** Review security checks during the update process (`ServiceWorkerUpdateChecker`).
*   **Navigation Preload:** Examine security implications (`NavigationPreloadManager`).

## 4. Code Analysis
*   `ServiceWorkerVersion` (Browser): Manages lifecycle, event dispatch (`DispatchFetchEvent`). Sends original request data (`fetch_request`) via Mojo.
*   `ServiceWorkerContextCore` (Browser): Manages registrations, storage (`ServiceWorkerStorage`). Contains scope matching logic (`FindRegistrationForScope`).
*   `ServiceWorkerControlleeRequestHandler` / `ServiceWorkerMainResourceLoaderInterceptor` (Browser): Browser-side request interception logic. Determines if a request should be handled by a service worker.
*   `ServiceWorkerGlobalScope` (Blink Renderer): Worker global scope.
    *   `DispatchFetchEventForMainResource`: Receives Mojo call, creates `FetchEvent` and `Request` objects using worker's `ScriptState`. **Crucial point for context creation.**
    *   `fetch()`: Initiates a fetch from the worker script. **Investigate how this call path interacts with core Blink fetch (`FetchManager`, `ResourceFetcher`) to determine security context.**
*   `FetchEvent` / `Request` (Blink): Event/request data objects.
*   `FetchRespondWithObserver` (Blink): Handles `event.respondWith()`.
*   `FetchManager` (`core/fetch/`): Core Blink logic for handling `fetch()` calls. **Likely determines security context for worker-initiated fetches.**
*   `ThreadableLoader` (`core/loader/`): Involved in loading resources based on fetch requests.

## 5. Areas Requiring Further Investigation
*   **Worker `fetch()` Context Determination:** **Trace the execution path from `fetch()` called within `ServiceWorkerGlobalScope::DispatchFetchEventForMainResource` into `FetchManager` and related core loading components.** Verify precisely how parameters like initiator origin, credentials mode, CORS mode, and CSP enforcement are determined. Confirm if/how the details of the original intercepted request (`fetch_request` received via Mojo) are used versus the worker's own context (ScriptState, SecurityOrigin), especially for extension service workers. (Focus for VRP: `1115438`, VRP2.txt#1234, #8913, #11875).
*   **Scope Matching Logic:** Audit scope matching algorithm in `ServiceWorkerContextCore::FindRegistrationForScope` and related functions for edge cases (e.g., URLs with fragments, different schemes).
*   **Update Process Security:** Analyze update logic (`ServiceWorkerUpdateChecker`) for potential bypasses or state inconsistencies.
*   **Interactions with Privacy Sandbox APIs.**
*   **Back-Forward Cache Interaction:** How does service worker state (registration, activation) interact with BFCache restoration?

## 6. Related VRP Reports
*   **CSP Bypass:** VRP: `598077`.
*   **SameSite Bypass:** VRP: `1115438`; VRP2.txt#7521.
*   **Extension SOP Bypass:** VRP2.txt#1234, #8913 (**Likely due to renderer/worker `fetch()` using extension context instead of original request context**).
*   **Mixed Content Bypass:** VRP2.txt#8497.
*   **Payment Request XSS:** VRP2.txt#276.
*   **Extension Context Bypass:** VRP2.txt#785.
*   **Information Leak:** VRP2.txt#7318, #7346 (Canvas), VRP2.txt#14773 (Cache).
*   **Origin Header Incorrect:** VRP2.txt#11875.

## 7. Cross-References
*   [content_security_policy.md](content_security_policy.md)
*   [privacy.md](privacy.md)
*   [payments.md](payments.md)
*   [extension_security.md](extension_security.md)
*   [navigation.md](navigation.md)
