# Geolocation API: The Browser-Side Implementation

The `GeolocationImpl` class is the final link in the chain for a geolocation request. It's the component that directly interacts with the `GeolocationProvider` to obtain location data from the operating system. It is instantiated by `GeolocationContext` for each incoming request from a renderer. The source is located in `services/device/geolocation/geolocation_impl.cc`.

## 1. Subscribing for Location Updates

The core responsibility of `GeolocationImpl` is to manage a subscription to the `GeolocationProvider`, which is a singleton that abstracts the platform-specific location services.

-   **`StartListeningForUpdates()`**: This is the key method. It calls `GeolocationProvider::GetInstance()->AddLocationUpdateCallback(...)`.
-   **Callback Registration**: It registers its own `OnLocationUpdate` method as the callback to be invoked by the provider whenever a new position is available.
-   **High Accuracy Hint**: The `high_accuracy_` flag, controlled by the renderer's `PositionOptions`, is passed to the provider. This allows the underlying OS to make a trade-off between precision and power consumption (e.g., using GPS vs. Wi-Fi triangulation). The subscription is updated if this hint changes via `SetHighAccuracyHint`.

## 2. Handling Data and Errors

-   **`OnLocationUpdate()`**: When the `GeolocationProvider` has a new position, it calls this method.
    -   The `GeopositionResult` (which could be a valid position or an error) is stored in `current_result_`.
    -   It then calls `ReportCurrentPosition()` to send the data to the renderer.
-   **`ReportCurrentPosition()`**: This method fulfills the pending `QueryNextPositionCallback` that the renderer is waiting on. It sends the `GeopositionResult` back over the Mojo connection. This is the final step that delivers the location data to the page's JavaScript.
-   **`OnPermissionRevoked()`**: If the `GeolocationContext` signals that permission has been revoked, this method is called. It immediately rejects the pending renderer callback with a `PERMISSION_DENIED` error, ensuring that access is cut off instantly.
-   **Connection Errors**: If the renderer-side remote disconnects (e.g., the tab closes), `OnConnectionError` is triggered. It notifies the `GeolocationContext`, which then destroys the `GeolocationImpl` instance, ensuring a clean teardown of the subscription.

## 3. DevTools Override Handling

While `GeolocationContext` manages the override state, `GeolocationImpl` is responsible for applying it.

-   **`SetOverride()`**: When called by the context, this method immediately stops the real location subscription (`geolocation_subscription_ = {};`). It then stores the mocked position and calls `OnLocationUpdate` with the overridden data, ensuring the renderer receives the mock instead of a real position.
-   **`ClearOverride()`**: This method discards the mocked position and calls `StartListeningForUpdates()` to re-establish the subscription to the real `GeolocationProvider`.

## Security Insights

-   **Abstraction from Hardware**: `GeolocationImpl` does not talk to the hardware. It talks to the `GeolocationProvider`. This is a crucial abstraction layer that isolates the core logic from the complexities of different operating systems' location APIs (Windows, Android, macOS, etc.).
-   **Single-Request Focus**: Each `GeolocationImpl` instance is dedicated to a single request from a single frame. This encapsulation prevents state from one request from leaking to another and simplifies lifecycle management.
-   **Instantaneous Revocation**: The path from `GeolocationContext::OnPermissionRevoked` to `GeolocationImpl::OnPermissionRevoked` ensures that user decisions to revoke permission are acted upon immediately, preventing any further location data from being sent to a page that is no longer authorized.
-   **Clean Teardown**: The destructor and the `OnConnectionError` handler ensure that pending callbacks in the renderer are always resolved (usually with an error) and that the subscription to the `GeolocationProvider` is properly torn down. This prevents zombie subscriptions that could waste battery or pose a privacy risk.

This completes the end-to-end analysis of the Geolocation API, from the JavaScript call in the renderer to the platform-level interaction in the browser process.