# Component: Service Workers

## 1. Component Focus
*   **Functionality:** Implements the Service Worker specification, allowing scripts to run in the background, intercept network requests (`FetchEvent`), manage caches, handle push notifications, and perform other tasks independent of a specific web page.
*   **Key Logic:** Lifecycle management (`ServiceWorkerVersion`, registration, updates, activation, termination), event dispatching (`FetchEvent`, `PushEvent`, `SyncEvent`, etc.), request interception and response generation (`FetchHandler`), scope management, storage (`ServiceWorkerStorage`), client management (`ServiceWorkerContainerHost`).
*   **Core Files:**
    *   `content/browser/service_worker/` (Browser-side implementation, e.g., `service_worker_version.cc`, `service_worker_context_core.cc`, `service_worker_controllee_request_handler.cc`)
    *   `third_party/blink/renderer/modules/service_worker/` (Renderer-side implementation, e.g., `service_worker_container.cc`, `service_worker_global_scope.cc`)
    *   `third_party/blink/public/mojom/service_worker/` (Mojo interfaces)

## 2. Potential Logic Flaws & VRP Relevance
*   **Request Interception Bypasses (FetchEvent):** Service workers intercepting network requests (`FetchEvent`) can modify or respond to requests, potentially bypassing security policies applied to the original request context.
    *   **VRP Pattern (CSP Bypass):** Intercepting requests and responding in a way that circumvents the controlled page's Content Security Policy (VRP: `598077`). See [content_security_policy.md](content_security_policy.md).
    *   **VRP Pattern (SameSite Cookie Bypass):** Handling a `FetchEvent` for a cross-site request and then re-issuing the fetch from the worker context, causing SameSite cookies (Lax/Strict) to be incorrectly sent (VRP: `1115438`, VRP2.txt#7521). See [privacy.md](privacy.md).
    *   **VRP Pattern (Mixed Content Bypass):** Potentially using service workers to facilitate loading of insecure content on secure pages (VRP2.txt#8497 - related to PWAs).
    *   **VRP Pattern (Origin Header):** Incorrectly setting the `Origin` header on requests made from within a `FetchEvent` handler (VRP2.txt#11875).
*   **Lifecycle Management Issues:** Race conditions or state inconsistencies during registration, update, activation, or termination.
    *   **VRP Pattern Concerns:** Could races during the update process allow installation of unintended worker scripts? Could termination logic (`StopWorker`, `Doom`) lead to resource leaks or inconsistent state?
*   **Cache Manipulation/Poisoning:** Flaws in cache interaction (`Cache API`) accessible from service workers.
    *   **VRP Pattern (Cache Side-Channel):** Using Cache API timing/behavior with Range requests to leak cross-origin resource size (VRP2.txt#14773).
*   **Origin/Scope Confusion:** Errors in enforcing the scope restrictions or validating origins associated with the service worker.
    *   **VRP Pattern Concerns:** Could a worker registered for one origin somehow intercept or influence requests for another origin due to faulty scope matching or IPC validation?
*   **Information Leaks:** Service workers potentially leaking information cross-origin through side channels or incorrect API implementations.
    *   **VRP Pattern (Pixel Data Leak):** Combining Service Worker interception with Canvas `drawImage` to read cross-origin pixel data (VRP2.txt#7318, #7346).
*   **Interaction with Other Features:** Exploits arising from interactions with other browser features like Payment Request API or extensions.
    *   **VRP Pattern (Payment Request XSS):** Persistent XSS via malicious PaymentRequest manifest *and* service worker installation (VRP2.txt#276). See [payments.md](payments.md).
    *   **VRP Pattern (Extension Context):** Extensions potentially interacting with service workers in unexpected ways (VRP2.txt#785 - Extension context isolation bypass involving SW). See [extension_security.md](extension_security.md).

## 3. Further Analysis and Potential Issues
*   **FetchEvent Security:** Deep dive into `FetchHandler` logic. How are security properties (origin, credentials mode, CSP, SameSite status) of the original request propagated and enforced when the worker handles the fetch (`respondWith`, `fetch`)? Are there inconsistencies compared to direct network fetches? (VRP: `1115438`).
*   **Lifecycle Races:** Analyze state transitions in `ServiceWorkerVersion` (`StartWorker`, `StopWorker`, activation logic) for potential race conditions, especially during updates or when multiple clients are involved.
*   **Scope Enforcement:** How is the service worker's scope rigorously enforced for interceptions? Can edge cases in URL parsing or matching bypass scope checks?
*   **Storage Security (`ServiceWorkerStorage`):** Review how service worker scripts and metadata are stored. Are there risks of corruption or unauthorized modification?
*   **Cross-Origin Communication:** Analyze `PostMessageToClient` and client handling logic (`ServiceWorkerContainerHost`) for potential cross-origin information leaks or control issues.
*   **Update Process:** Review the security of the service worker update process. Can an attacker interfere with updates to install malicious scripts?
*   **Navigation Preload:** Examine security implications. Can preload requests leak information or bypass security checks?

## 4. Code Analysis
*   `ServiceWorkerVersion`: Represents a specific version of a service worker script. Manages lifecycle (starting, stopping, events). Key methods: `StartWorker`, `StopWorker`, `DispatchFetchEvent`.
*   `ServiceWorkerContextCore`: Manages service worker registrations and versions for a storage partition.
*   `ServiceWorkerControlleeRequestHandler` / `ServiceWorkerMainResourceLoaderInterceptor`: Handle request interception for navigations and subresources. Contain logic for dispatching `FetchEvent`.
*   `FetchHandler`: Logic within the service worker script handling the `FetchEvent`. Crucial for bypasses related to interception (VRP: `1115438`, `598077`).
*   `ServiceWorkerContainerHost`: Browser-side representation of clients controlled by a service worker. Handles client management (`ClaimClients`, `GetClient`) and message passing (`PostMessageToClient`).
*   `PaymentManager`: Interaction point for Payment Request API, potentially involved in manifest/SW XSS (VRP2.txt#276).
*   Cache Storage related code (`CacheStorageCache`, `CacheStorageManager`): Interaction point for cache side-channels (VRP2.txt#14773).

## 5. Areas Requiring Further Investigation
*   **FetchEvent Security Policy Enforcement:** Systematically verify that all relevant security policies (CSP, SameSite, Mixed Content, CORS) are correctly applied to requests handled within or initiated from a `FetchEvent` handler.
*   **Scope Matching Logic:** Audit the scope matching algorithm (`ServiceWorkerContextCore::FindRegistrationForScope`, `ScopeMatches`) for edge cases and potential bypasses.
*   **Update Process Security:** Analyze the script comparison and update logic for vulnerabilities that could allow installation of unintended scripts.
*   **Interactions with Privacy Sandbox APIs:** Explore how service workers interact with newer privacy-focused APIs.
*   **Back-Forward Cache Interaction:** Ensure state consistency and security when pages with active service workers are restored from BFCache.

## 6. Related VRP Reports
*   **CSP Bypass:** VRP: `598077` (Intercept bypass).
*   **SameSite Bypass:** VRP: `1115438`; VRP2.txt#7521 (FetchEvent bypass).
*   **Mixed Content Bypass:** VRP2.txt#8497 (PWA/SW interaction).
*   **Payment Request XSS:** VRP2.txt#276 (Manifest + SW).
*   **Extension Context Bypass:** VRP2.txt#785 (Interaction with extension import).
*   **Information Leak:** VRP2.txt#7318, #7346 (Canvas pixel leak), VRP2.txt#14773 (Cache API size leak).
*   **Origin Header Incorrect:** VRP2.txt#11875.

*(See also [content_security_policy.md](content_security_policy.md), [privacy.md](privacy.md), [payments.md](payments.md))*
