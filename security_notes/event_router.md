# Security Analysis of extensions/browser/event_router.cc

## 1. Overview

The `EventRouter` is a foundational `KeyedService` within the extensions subsystem, acting as the central hub for all asynchronous event communication from the browser to extensions. It manages event listeners from various contexts (background pages, service workers, content scripts, tabs) and is responsible for dispatching events like `tabs.onUpdated`, `webNavigation.onCompleted`, and `runtime.onMessage` to the correct recipients.

Given that it handles the flow of potentially sensitive data (e.g., the URL of a navigated tab) to extensions, its security is paramount. A flaw in the `EventRouter` could lead to information leaks (an extension receiving an event it shouldn't have), denial of service (an event storm), or incorrect extension behavior.

## 2. Core Security Concepts & Mechanisms

### 2.1. Listener Registration and Tracking

The `EventRouter` maintains a comprehensive map of all active event listeners.

-   **`EventListenerMap`**: This is the core data structure, mapping event names to a list of `EventListener` objects. Each `EventListener` object contains the `extension_id`, the `RenderProcessHost` of the listener, the `listener_url` (for web pages), and service worker details if applicable.
-   **Context-Specific Registration**: The `EventRouter` exposes different Mojo IPC methods for adding listeners from different contexts (`AddListenerForMainThread`, `AddListenerForServiceWorker`). This is important because the security context and lifecycle of a service worker are different from that of a tab or background page.
-   **Lazy Listeners**: For non-persistent background contexts (event pages and service workers), the `EventRouter` maintains a separate set of "lazy" listeners persisted in `ExtensionPrefs` (`kRegisteredLazyEvents`, `kRegisteredServiceWorkerEvents`). When an event arrives for a lazy listener, the `EventRouter` uses `LazyEventDispatchUtil` to wake the background context before dispatching the event. This prevents events from being dropped while ensuring event pages aren't running unnecessarily.

**Security Criticality**: The integrity of the `EventListenerMap` is crucial. It must accurately reflect the state of active listeners in the renderers. The `EventRouter` acts as a `RenderProcessHostObserver`, correctly cleaning up listeners when a process exits (`RenderProcessExited`). This prevents a scenario where the router attempts to dispatch an event to a dead process.

### 2.2. Secure Event Dispatching

When `BroadcastEvent` or `DispatchEventToExtension` is called, the `EventRouter` begins a multi-stage dispatch process, managed primarily by `EventDispatchHelper`.

-   **`WillDispatchCallback`**: The `Event` object contains an optional `WillDispatchCallback`. This is a powerful security mechanism. Before dispatching the event to a specific extension, the router invokes this callback. The callback can inspect the target `BrowserContext`, `Extension`, and event filter, and then decide if the event should proceed. It can also *modify* the event arguments on a per-target basis. This is used by APIs like `webRequest` to ensure an extension only sees events from profiles it has access to (e.g., respecting incognito settings).
-   **Permission and Feature Checks**: `EventDispatchHelper::CheckFeatureAvailability` performs a critical check to ensure the target context (e.g., a specific extension in a specific process) has the necessary feature and permission to receive the event. For example, an extension can't receive an `alarms.onAlarm` event if it doesn't have the `alarms` permission. This check prevents information leaks to extensions that shouldn't have it.
-   **URL-Based Filtering**: For events that are restricted by host permissions (e.g., `webNavigation`), the `Event` object carries an `event_url`. The `EventDispatchHelper` will only dispatch the event to extensions whose host permissions match this URL. This is a primary security boundary preventing extensions from getting navigation details about sites they don't have access to.
-   **Process and Context Targeting**: The router ensures events are dispatched to the correct `RenderProcessHost` and, within that, to the correct context (e.g., a specific service worker thread vs. the main thread). The Mojo IPC interfaces (`mojom::EventDispatcher`) are per-process, and the `DispatchEvent` call includes the necessary identifiers (`worker_thread_id`, `host_id`) to route it correctly on the renderer side.

**Security Criticality**: The combination of the `WillDispatchCallback`, feature checks, and host permission enforcement forms the core of the `EventRouter`'s security model. A flaw in any of these checks could lead to an extension receiving an event with sensitive data (like a URL) that it is not authorized to see.

### 2.3. Event Acking and Lifecycle Management

To manage the lifecycle of lazy background contexts, the `EventRouter` has a system for tracking in-flight events.

-   **`IncrementInFlightEvents`**: When an event is dispatched to a lazy background page or service worker, this method is called. It increments a keepalive count on the `ProcessManager` (for event pages) or stores tracking info in `EventAckData` (for service workers). This prevents the browser from shutting down the background context while it's processing the event.
-   **`OnEventAck` / `DecrementInFlightEvents...`**: When the renderer-side event handler completes, it sends an ACK back to the browser. This triggers a decrement of the keepalive count or removes the entry from `EventAckData`, allowing the background context to be suspended again if it has no other work to do.

**Security Criticality**: This mechanism is primarily for resource management, but it has security implications. If the ACK mechanism were to fail or be bypassed, a malicious extension could potentially keep its background page alive indefinitely by never acking an event, creating a persistent process. The router also includes timeouts (`kEventAckMetricTimeLimit`) to detect and log events that are not acked in a timely manner, which helps diagnose such issues.

## 4. Potential Attack Vectors & Security Risks

1.  **Permission Bypass**: A flaw in `EventDispatchHelper::CheckFeatureAvailability` or in the host permission check for URL-sensitive events could allow an extension to receive events it shouldn't, leading to information leaks.
2.  **Cross-Profile Data Leakage**: The `restrict_to_browser_context` field in the `Event` object is critical for preventing events from an incognito session from leaking to an extension's regular process (if it's not in split mode). A bug in how this restriction is applied would be a serious privacy and security issue.
3.  **Listener Map Desynchronization**: If the router fails to properly clean up listeners when a process dies (`RenderProcessExited`), it could lead to attempts to dispatch events to non-existent processes. While this is unlikely to be exploitable, it indicates a state-management problem.
4.  **Denial of Service**: A malicious extension that registers for a high-frequency event (e.g., `webRequest.onAuthRequired`) and performs slow processing could cause performance issues. While the `EventRouter` itself is efficient, it relies on the underlying extension to be well-behaved. The event acking mechanism provides some visibility into misbehaving extensions.

## 5. Conclusion

The `EventRouter` is a robust and security-conscious component for brokering events. Its security model is based on a multi-layered filtering approach: first by event name, then by permissions and features, then by browser context, and finally by URL-based host permissions. The use of a `WillDispatchCallback` provides a flexible and powerful way to enforce per-event, per-target security policies. The lifecycle management of listeners and in-flight events is well-handled through `RenderProcessHostObserver`, ensuring state is cleaned up correctly. The most critical areas for security review are the `EventDispatchHelper` and the logic that restricts event dispatch across browser contexts.