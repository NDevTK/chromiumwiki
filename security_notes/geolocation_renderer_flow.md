# Geolocation API: The Renderer-Side Flow

This document outlines the initial phase of a geolocation request, as it occurs within the Blink renderer process. The analysis is based on the `Geolocation` class defined in `third_party/blink/renderer/core/geolocation/geolocation.h`.

## 1. Entry Point: `navigator.geolocation`

When a web page executes JavaScript like `navigator.geolocation.getCurrentPosition(...)` or `navigator.geolocation.watchPosition(...)`, it interacts with the `Geolocation` object in the renderer.

-   **`getCurrentPosition(...)`**: This method is for a one-time position request. It creates a `GeoNotifier` object to manage the request's callbacks (for success or error) and its options (e.g., timeout, `enableHighAccuracy`).
-   **`watchPosition(...)`**: This method sets up a persistent watcher. It also creates a `GeoNotifier` but adds it to a list of watchers that will receive continuous updates as the user's position changes.

## 2. The `Geolocation` Class

This class is the central coordinator for geolocation operations within the renderer.

-   **State Management**: It maintains lists of active one-shot requests (`one_shots_`) and persistent watchers (`watchers_`).
-   **Cached Position**: It can store the `last_position_` to immediately satisfy requests that don't require a fresh location and meet the `maximumAge` option.

## 3. Requesting Permission and Data: The Mojo Connection

The most critical function in this class is `UpdateGeolocationState()`, which is responsible for initiating the request to the browser process.

-   **`EnsureGeolocationConnection()`**: This function is called to establish a Mojo connection to the browser's `GeolocationService`. This is an asynchronous operation. A `HeapMojoRemote<mojom::blink::GeolocationService>` is used to manage this connection.
-   **Permission Check**: As part of establishing the connection, the renderer implicitly asks the browser to check the permission status for the origin. The browser's `PermissionController` handles this check.
-   **Callback**: The renderer provides a callback, `OnGeolocationPermissionStatusUpdated`, which the browser will invoke with the result of the permission check (`granted`, `denied`, or `prompt`).

## 4. Handling the Permission Decision

The flow diverges based on the permission status returned from the browser:

-   **If `PermissionStatus` is `DENIED`**:
    -   The `HandlePermissionError` function is called.
    -   All pending notifiers (both one-shots and watchers) are immediately rejected with a `PERMISSION_DENIED` error.
    -   The Mojo connection is torn down.

-   **If `PermissionStatus` is `GRANTED`**:
    -   The renderer proceeds to request the actual location data.
    -   It calls `QueryNextPosition()` on its `device::mojom::blink::Geolocation` remote. This sends an IPC message to the browser's device service, asking it to start providing location updates.
    -   The browser's `GeolocationImpl` service will then use the underlying operating system's location APIs to get the data.

-   **If `PermissionStatus` is `PROMPT`**:
    -   The browser process takes over. The `PermissionControllerDelegate` will display a prompt to the user.
    -   The renderer process essentially waits. No location data is sent.
    -   If the user grants permission, the browser will notify the renderer, and the flow will proceed as if the status were `GRANTED` from the start.
    -   If the user denies permission, the flow proceeds as if the status were `DENIED`.

## 5. Receiving Location Data

When the browser's device service obtains a new location, it sends it back to the renderer via the Mojo connection, invoking `OnPositionUpdated`.

-   The new `Geoposition` is cached in `last_position_`.
-   The `MakeSuccessCallbacks` function is called, which iterates through all the `GeoNotifier` objects (both one-shots and watchers).
-   It checks if the new position meets the criteria of each notifier (e.g., accuracy).
-   For each matching notifier, its success callback is invoked, delivering the `Geoposition` object to the original JavaScript caller.
-   One-shot notifiers are then removed, while watchers remain to receive future updates.

## Security Notes

-   **Separation of Privilege**: The renderer process never accesses location hardware directly. It is entirely dependent on the browser process, which acts as a gatekeeper.
-   **Asynchronous Permission Model**: The permission check is asynchronous, preventing the renderer from blocking while waiting for a user decision.
-al **Error Handling**: The class has robust error handling for various scenarios, including timeouts (`RequestTimedOut`), permission denial, and fatal errors from the device service. This ensures that JavaScript promises or callbacks are always resolved, preventing stalled states in the web application.