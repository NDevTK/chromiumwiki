# Component: Shared Workers

## 1. Component Focus
*   **Functionality:** Implements the Shared Worker specification, allowing scripts to run in a background process that can be accessed by multiple browser contexts (tabs, iframes) from the same origin. Facilitates shared state and communication between same-origin documents.
*   **Key Logic:** Handling the `SharedWorker` constructor, starting/managing the worker process (`SharedWorkerHost`, `SharedWorkerServiceImpl`), managing connections from multiple clients (`SharedWorkerConnectorImpl`), handling message passing (`MessagePort`), script loading and execution within the worker context (`SharedWorkerGlobalScope`).
*   **Core Files:**
    *   `third_party/blink/renderer/core/workers/shared_worker.cc`: Renderer-side `SharedWorker` object.
    *   `third_party/blink/renderer/core/workers/shared_worker_global_scope.cc`: The global scope within the running shared worker.
    *   `content/browser/worker_host/shared_worker_host.cc`: Browser-side representation of the shared worker instance.
    *   `content/browser/worker_host/shared_worker_service_impl.cc`: Manages shared worker instances for a storage partition.
    *   `content/browser/worker_host/shared_worker_connector_impl.cc`: Handles connection requests from renderers.

## 2. Potential Logic Flaws & VRP Relevance
*   **Isolation/Origin Enforcement:** Flaws allowing cross-origin documents to connect to or influence a shared worker, or allowing the worker to access data from origins other than its own.
*   **Lifecycle Management:** Race conditions or state inconsistencies during worker startup, connection establishment, client detachment, or termination, potentially leading to UAFs or logic bugs.
*   **Communication Security (`MessagePort`):** Vulnerabilities in message port handling, allowing unintended communication or data leakage between clients connected to the same worker, or between the worker and clients.
*   **Resource Exhaustion (DoS):** Creating excessive shared workers or message ports, or infinite loops within a worker, consuming system resources.
*   **Information Leaks:** The shared worker potentially leaking sensitive information (e.g., about connected clients, internal state) accessible to its clients.
    *   **VRP Pattern (Local File Read):** A vulnerability allowed a shared worker to read local files, potentially by exploiting incorrect origin checks or file URL handling within the worker context (VRP2.txt#670). See [file_system_access.md](file_system_access.md).

## 3. Further Analysis and Potential Issues
*   **Origin Checks:** Verify that only documents from the *exact same origin* as the shared worker script can connect (`SharedWorkerConnectorImpl`, `SharedWorkerServiceImpl`). How are origins compared?
*   **Connection Handling:** Analyze the process of connecting a new client to an existing shared worker (`SharedWorkerHost::AddClient`). Are there race conditions?
*   **Script Loading:** How is the worker script fetched and executed? Does it respect CSP and other security policies?
*   **`file://` URL Handling:** How do shared workers behave when initiated from or attempting to access `file://` URLs? Are origin checks correctly applied to prevent reading arbitrary local files? (VRP2.txt#670).
*   **Shared State Security:** Analyze how shared state is managed within the worker and ensure it cannot be manipulated or accessed improperly by different clients.

## 4. Code Analysis
*   `SharedWorker` (Blink): Constructor initiates connection.
*   `SharedWorkerConnectorImpl`: Renderer-side connector to the browser service.
*   `SharedWorkerServiceImpl`: Browser-side service managing worker instances. Checks origin equality (`CanConnect`).
*   `SharedWorkerHost`: Browser-side representation of a running worker instance. Manages clients (`AddClient`, `RemoveClient`).
*   `SharedWorkerGlobalScope` (Blink): Execution context within the worker. Handles events like `onconnect`.

## 5. Areas Requiring Further Investigation
*   **Origin Equality Checks:** Deep dive into `SharedWorkerServiceImpl::CanConnect` and related origin comparison logic for edge cases (e.g., opaque origins, file URLs).
*   **Lifecycle Race Conditions:** Test scenarios involving rapid connection/disconnection of clients, worker termination, and script updates.
*   **`file://` URL Security:** Specifically test scenarios involving shared workers created from or accessing `file://` URLs to ensure local file access restrictions are enforced (VRP2.txt#670).
*   **MessagePort Security:** Analyze `MessageChannel` and `MessagePort` usage within shared workers for potential vulnerabilities.

## 6. Related VRP Reports
*   VRP2.txt#670 (Local file read via SharedWorker)

*(See also [service_workers.md](service_workers.md), [ipc.md](ipc.md), [site_isolation.md](site_isolation.md))*
