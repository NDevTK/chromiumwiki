# Component: Service Workers

## 1. Component Focus
*   **Functionality:** Implements the Service Worker specification, allowing scripts to run in the background, intercept network requests (`FetchEvent`), manage caches, handle push notifications, and perform other tasks independent of a specific web page.
*   **Key Logic:** Lifecycle management (`ServiceWorkerVersion`), event dispatching (`ServiceWorkerVersion::DispatchFetchEvent`, `ServiceWorkerGlobalScope::DispatchFetchEventForMainResource`), request interception and response generation (`FetchEvent`, `FetchRespondWithObserver`), scope management, storage (`ServiceWorkerStorage`), client management (`ServiceWorkerContainerHost`).
*   **Core Files:**
    *   `content/browser/service_worker/` (Browser-side implementation, e.g., `service_worker_version.cc`, `service_worker_context_core.cc`, `service_worker_controllee_request_handler.cc`)
    *   `third_party/blink/renderer/modules/service_worker/` (Renderer-side implementation, e.g., `service_worker_global_scope.cc`, `fetch_event.cc`, `fetch_respond_with_observer.cc`)
    *   `third_party/blink/public/mojom/service_worker/` (Mojo interfaces)

## 2. Potential Logic Flaws & VRP Relevance
*   **Request Interception Bypasses (FetchEvent):** Service workers intercepting network requests (`FetchEvent`) can modify or respond to requests, potentially bypassing security policies applied to the original request context.
    *   **VRP Pattern (CSP Bypass):** Intercepting requests and responding in a way that circumvents the controlled page's Content Security Policy (VRP: `598077`). See [content_security_policy.md](content_security_policy.md).
    *   **VRP Pattern (SameSite Cookie Bypass):** Handling a `FetchEvent` for a cross-site request and then re-issuing the fetch from the worker context, causing SameSite cookies (Lax/Strict) to be incorrectly sent (VRP: `1115438`, VRP2.txt#7521). See [privacy.md](privacy.md).
    *   **VRP Pattern (Mixed Content Bypass):** Potentially using service workers to facilitate loading of insecure content on secure pages (VRP2.txt#8497 - related to PWAs).
    *   **VRP Pattern (Origin Header):** Incorrectly setting the `Origin` header on requests made from within a `FetchEvent` handler (VRP2.txt#11875).
    *   **VRP Pattern (Extension SOP Bypass):** Extension service workers intercepting requests and potentially re-issuing them with elevated extension privileges, bypassing Same-Origin Policy for the original initiator.
        1. Browser (`ServiceWorkerVersion::DispatchFetchEvent`) sends original request details (`FetchAPIRequestPtr`) via Mojo to the renderer worker process. This dispatch logic doesn't alter context based on worker type.
        2. Renderer (`ServiceWorkerGlobalScope::DispatchFetchEventForMainResource`) receives the Mojo call.
        3. It creates a Blink `Request` object using the **worker's ScriptState** but populating it with data from the received `FetchAPIRequestPtr`.
        4. It creates a `FetchEvent` containing this `Request` object and dispatches it to the worker's JavaScript scope.
        5. The extension's `onfetch` handler calls `fetch()` (potentially on `event.request`).
        6. **This `fetch()` executes within the extension worker's context.** Security checks (SOP, host permissions) are likely performed based on the **extension's origin and permissions**, not the original request's initiator origin or context, thus allowing the bypass.
        (Related to VRP2.txt#1234, #8913). See [extension_security.md](extension_security.md).
*   **Lifecycle Management Issues:** Race conditions or state inconsistencies during registration, update, activation, or termination.
*   **Cache Manipulation/Poisoning:**
    *   **VRP Pattern (Cache Side-Channel):** (VRP2.txt#14773).
*   **Origin/Scope Confusion:** Errors enforcing scope or validating origins.
*   **Information Leaks:**
    *   **VRP Pattern (Pixel Data Leak):** (VRP2.txt#7318, #7346).
*   **Interaction with Other Features:** Exploits arising from interactions with other browser features.
    *   **VRP Pattern (Payment Request XSS):** (VRP2.txt#276). See [payments.md](payments.md).
    *   **VRP Pattern (Extension Context):** (VRP2.txt#785 - Isolation bypass).

## 3. Further Analysis and Potential Issues
*   **FetchEvent Security (Renderer):** Deep dive into `fetch()` implementation within worker contexts. How exactly are credentials mode, CORS mode, referrer policy, and initiator origin from the original request (`FetchAPIRequest`) applied or potentially overridden by the worker's context/privileges when the worker itself initiates a fetch? Are there differences for extension vs. web workers?
*   **Lifecycle Races:** Analyze state transitions.
*   **Scope Enforcement:** Audit scope matching.
*   **Storage Security:** Review script storage.
*   **Cross-Origin Communication:** Analyze `PostMessageToClient`.
*   **Update Process:** Review update security.
*   **Navigation Preload:** Examine security implications.

## 4. Code Analysis
*   `ServiceWorkerVersion`: Browser-side representation. Manages lifecycle, event dispatch (`DispatchFetchEvent`). Sends original request data via Mojo.
*   `ServiceWorkerContextCore`: Browser-side. Manages registrations.
*   `ServiceWorkerControlleeRequestHandler` / `ServiceWorkerMainResourceLoaderInterceptor`: Browser-side request interception logic.
*   `ServiceWorkerGlobalScope` (Blink): Renderer-side worker global scope. Implements Mojo `ServiceWorker` interface, including `DispatchFetchEventForMainResource`. **Creates `FetchEvent` and `Request` objects using worker's `ScriptState`.**
*   `FetchEvent` / `Request` (Blink): Renderer-side objects representing the event and request data.
*   `FetchRespondWithObserver` (Blink): Handles `event.respondWith()`.
*   `ServiceWorkerContainerHost`: Browser-side client representation.
*   Other interaction points: `PaymentManager`, Cache Storage.

## 5. Areas Requiring Further Investigation
*   **Renderer `fetch()` Implementation for Workers:** **Systematically verify how `fetch()` initiated from within an `onfetch` handler determines its security context (origin, credentials, mode, initiator) – does it correctly use the intercepted request's details or improperly inherit from the worker (especially extension worker) context?**
*   **Scope Matching Logic:** Audit scope matching algorithm.
*   **Update Process Security:** Analyze update logic.
*   **Interactions with Privacy Sandbox APIs.**
*   **Back-Forward Cache Interaction.**

## 6. Related VRP Reports
*   **CSP Bypass:** VRP: `598077`.
*   **SameSite Bypass:** VRP: `1115438`; VRP2.txt#7521.
*   **Extension SOP Bypass:** VRP2.txt#1234, #8913 (**Likely due to renderer/worker `fetch()` using extension context instead of original request context**).
*   **Mixed Content Bypass:** VRP2.txt#8497.
*   **Payment Request XSS:** VRP2.txt#276.
*   **Extension Context Bypass:** VRP2.txt#785.
*   **Information Leak:** VRP2.txt#7318, #7346 (Canvas), VRP2.txt#14773 (Cache).
*   **Origin Header Incorrect:** VRP2.txt#11875.

*(See also [content_security_policy.md](content_security_policy.md), [privacy.md](privacy.md), [payments.md](payments.md), [extension_security.md](extension_security.md))*
