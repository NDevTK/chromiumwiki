# Component: Push Messaging API

## 1. Component Focus
*   **Functionality:** Implements the Push API ([Spec](https://www.w3.org/TR/push-api/)), allowing web applications (via their service worker) to receive messages pushed from their server, even when the application is not active in the browser. Requires user permission.
*   **Key Logic:** Handling subscription requests (`PushManager.subscribe`), managing subscriptions (`PushSubscription`), interacting with platform push services (e.g., FCM, APNS), delivering push messages to the correct service worker (`push` event).
*   **Core Files:**
    *   `third_party/blink/renderer/modules/push_messaging/`: Renderer-side API implementation (`PushManager`, `PushSubscription`).
    *   `content/browser/push_messaging/push_messaging_manager.cc`: Browser-side management of subscriptions.
    *   `content/browser/push_messaging/push_messaging_router.cc`: Routes incoming messages to the correct service worker.
    *   `components/gcm_driver/`: Interaction with Google Cloud Messaging (GCM), often used as the underlying push service.
    *   Interaction with `ServiceWorkerContext` and permission systems.

## 2. Potential Logic Flaws & VRP Relevance
*   **Permission Bypass/Origin Confusion:** Flaws allowing a site to subscribe for pushes without proper permission, or receive pushes intended for a different origin.
    *   **VRP Pattern (Origin Check Bypass):** Lack of origin checks in Mojo IPC handling allowed compromised renderers to interact with the Push Messaging API on behalf of other origins (VRP: `1275626`). See [ipc.md](ipc.md), [mojo.md](mojo.md).
*   **Subscription Management Issues:** Errors in creating, updating, or removing subscriptions, potentially leading to duplicate messages, missed messages, or DoS.
*   **Message Delivery Flaws:** Incorrectly routing push messages to the wrong service worker, or failing to deliver messages. Race conditions in service worker startup/shutdown interacting with message delivery.
*   **Information Leaks:** Leaking subscription endpoints, application server keys, or message contents.
*   **Resource Exhaustion (DoS):** Excessive subscriptions or push messages overwhelming browser or system resources.

## 3. Further Analysis and Potential Issues
*   **Origin Validation:** Ensure rigorous origin checks are performed at the browser process boundary (`PushMessagingManager`, related Mojo interfaces) for all subscription and messaging operations (VRP: `1275626`).
*   **Permission Flow:** Analyze the permission prompt and storage mechanism (`permissions::PermissionManager`) for push subscriptions. Is consent clearly obtained and securely stored?
*   **Subscription Uniqueness/Management:** How are subscription endpoints generated? How is uniqueness ensured? How are stale or revoked subscriptions handled?
*   **Message Routing (`PushMessagingRouter`):** Analyze the logic for mapping incoming platform push messages (e.g., from FCM) to the correct service worker registration and dispatching the `push` event. Look for race conditions with service worker lifecycle events.
*   **Interaction with Platform Push Services:** Analyze the security assumptions and data flow when interacting with services like FCM.

## 4. Code Analysis
*   `PushManager` (Blink): Renderer-side API (`subscribe`, `getSubscription`, `permissionState`).
*   `PushMessagingManager` (Browser): Handles subscription logic, interacts with permission manager and GCM driver. Check origin validation for Mojo calls (VRP: `1275626`).
*   `PushMessagingRouter` (Browser): Routes incoming push messages.
*   `GCMDriver` / `InstanceIDDriver`: Interfaces for interacting with platform push services.
*   `ServiceWorkerRegistration`: Holds push subscription information. `ServiceWorkerVersion::DispatchPushEvent`.

## 5. Areas Requiring Further Investigation
*   **Mojo Interface Security:** Audit the `blink.mojom.PushMessaging` interface and its browser-side implementation (`PushMessagingManager`) for missing origin/permission checks (VRP: `1275626`).
*   **Subscription Renewal/Expiration:** How is subscription validity handled? Are there edge cases around renewal?
*   **Message Payload Handling:** How is push message payload data handled? Are there size limits? Is decryption handled securely if applicable?
*   **Error Handling:** Analyze error reporting for subscription failures or message delivery issues.

## 6. Related VRP Reports
*   VRP: `1275626` (Origin check bypass in Mojo IPC)

*(See also [service_workers.md](service_workers.md), [permissions.md](permissions.md), [ipc.md](ipc.md), [mojo.md](mojo.md))*
