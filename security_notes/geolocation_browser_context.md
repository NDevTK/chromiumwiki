# Geolocation API: The Browser-Side Context

After a geolocation request leaves the renderer, it arrives at the `GeolocationContext` in the browser's device service. This class acts as a central hub for managing all active geolocation operations. Its implementation is found in `services/device/geolocation/geolocation_context.cc`.

## 1. Entry Point: `BindGeolocation`

The `GeolocationContext` exposes a Mojo interface, `mojom::GeolocationContext`, which the renderer's `GeolocationService` connects to. The key method is `BindGeolocation`.

-   **Functionality**: When a new request arrives from a renderer process (representing a specific frame or worker), this method is invoked.
-   **Instance Creation**: Its primary job is to instantiate a new `GeolocationImpl` object for *each* request. It passes the Mojo receiver endpoint, the requesting URL, and a reference to itself to the new `GeolocationImpl` instance.
-   **Lifecycle Management**: The `GeolocationContext` maintains a list (`impls_`) of all the `GeolocationImpl` objects it has created, effectively tracking all active location requests.

## 2. Managing the `GeolocationImpl` Lifecycle

The `GeolocationContext` is responsible for creating and destroying `GeolocationImpl` instances.

-   **Connection Errors**: If the Mojo connection to a specific renderer frame is lost (e.g., the tab is closed), the `GeolocationImpl` object associated with that frame will notify the context via `OnConnectionError`. The context then removes that implementation from its list and destroys it.
-   **Permission Revocation**: This is a critical security function. The `PermissionController` can notify the `GeolocationContext` that a permission has been revoked for a specific origin via `OnPermissionRevoked`.
    -   The context iterates through all its active `GeolocationImpl` instances.
    -   For each instance whose requesting URL matches the revoked origin, it calls `impl->OnPermissionRevoked()`. This sends a `PERMISSION_DENIED` error back to the renderer, ensuring the page's callbacks are rejected.
    -   The context then immediately destroys the `GeolocationImpl` object, terminating the location subscription. This ensures that a site cannot continue to receive location data after the user has revoked its permission.

## 3. DevTools Overrides

The `GeolocationContext` also plays a key role in enabling developers to test their applications with mock location data.

-   **`SetOverride`**: This method allows DevTools to push a specific `GeopositionResult` (which can be a valid position or an error) to the context. The context stores this override.
-   **`ClearOverride`**: This removes the active override.
-   **Propagation**: When an override is set, the `GeolocationContext` iterates through all of its existing `GeolocationImpl` instances and calls `impl->SetOverride()`. For any *new* `GeolocationImpl` instances created while an override is active, the override is applied immediately upon creation. This ensures that all frames are consistently receiving the mocked data.

## Security Posture

-   **Centralized Control**: By managing all `GeolocationImpl` instances, the `GeolocationContext` provides a single point of control for enforcing high-level policies, such as permission revocations and DevTools overrides.
-   **Clean Separation**: It acts as a factory and manager, cleanly separating the logic of handling Mojo connections and lifecycles from the logic of actually obtaining location data (which is the job of `GeolocationImpl`).
-   **Robust Lifecycle Management**: The context ensures that `GeolocationImpl` objects do not leak. They are created on-demand and are reliably destroyed either when the requesting frame goes away or when its permission is explicitly revoked. This prevents stale or unauthorized location subscriptions from persisting in the background.