# Component: Web Bluetooth

## 1. Component Focus
*   **Functionality:** Implements the Web Bluetooth API ([Spec](https://webbluetoothcg.github.io/web-bluetooth/)), allowing web pages to discover and communicate with nearby Bluetooth Low Energy (BLE) devices after user consent via a chooser dialog.
*   **Key Logic:** Handling API calls (`requestDevice`, `getAvailability`, GATT operations like `connect`, `getPrimaryService`, `getCharacteristic`, `readValue`, `writeValue`), managing device discovery (`BluetoothDeviceChooserController`), displaying the chooser UI (`BluetoothChooserBubbleController`, `BluetoothChooserAndroid`), handling permissions (`BluetoothScanningPromptController`, `ChooserContextBase`, `BluetoothChooserContext`), interacting with the platform's Bluetooth stack (`device/bluetooth`).
*   **Core Files:**
    *   `content/browser/bluetooth/`: Core browser-side logic (e.g., `web_bluetooth_service_impl.cc`, `bluetooth_device_chooser_controller.cc`).
    *   `components/permissions/contexts/bluetooth_chooser_context.cc`: Permission management for chosen devices.
    *   `components/permissions/contexts/bluetooth_scanning_permission_context.cc`: Permission management for scanning.
    *   `chrome/browser/ui/bluetooth/`: UI controllers (e.g., `bluetooth_chooser_bubble_controller.cc`).
    *   `chrome/browser/ui/views/bluetooth/`: Desktop UI views (e.g., `bluetooth_chooser_bubble_view.cc`).
    *   `components/permissions/android/bluetooth_chooser_android.cc`/`.java`: Android chooser UI implementation.
    *   `device/bluetooth/`: Platform abstraction layer (`bluetooth_adapter.h`, `bluetooth_device.h`, `bluetooth_gatt_connection.h`).
    *   `third_party/blink/renderer/modules/bluetooth/`: Renderer-side API implementation (`Bluetooth.idl`, `BluetoothDevice.idl`, `BluetoothRemoteGATTServer.idl`).

## 2. Potential Logic Flaws & VRP Relevance
Web Bluetooth vulnerabilities often relate to the chooser UI (origin display) or potential flaws in managing device connections and permissions.

*   **Chooser UI Spoofing/Origin Confusion:** The Bluetooth device chooser dialog failing to correctly display the requesting origin, potentially tricking users into granting permissions to the wrong site.
    *   **VRP Pattern (Opaque Origin):** Dialog showing empty origin string (`://`) when initiated from an opaque origin (e.g., sandboxed iframe, data URL). Common issue affecting multiple chooser types. Requires chooser UI code to handle opaque origins correctly, likely by showing the top-level origin. (VRP: `40061374`, `40061373`; VRP2.txt#8904, #9771).
    *   **VRP Pattern (Permission Delegation - Android):** On Android, chooser dialog (`BluetoothChooserAndroid`) incorrectly showing the *initiator iframe's* origin instead of the *top-level page's* origin when permission delegation is used (via Permissions Policy `bluetooth=()`). This leads to origin spoofing as the permission granted actually applies to the top-level origin. (VRP2.txt#10263). The root cause is likely passing the initiating frame's origin to `url_formatter::FormatOriginForSecurityDisplay` instead of retrieving and using the top-level frame's origin.

*   **Permission Bypass:** Circumventing user consent requirements for scanning (`requestLEScan` - less common API?) or connecting (`requestDevice`) to devices. (Requires strict enforcement of user gesture requirements and permission context checks).

*   **Information Leaks:** Leaking sensitive information about nearby devices (beyond what the spec allows during discovery) or the user's Bluetooth configuration or paired devices.

*   **Device Interaction Flaws:** Vulnerabilities in handling communication with BLE devices (GATT services/characteristics). Incorrect state management, validation errors, or memory safety issues in the `device/bluetooth` layer or platform backends during GATT operations.

*   **Race Conditions:** Issues during device discovery (`BluetoothDiscoverySession`), chooser display, connection establishment (`BluetoothGattConnection`), or permission handling.

## 3. Further Analysis and Potential Issues
*   **Origin Display in Chooser (All Platforms):** **Verify that `BluetoothChooserBubbleController` (Desktop) and `BluetoothChooserAndroid` always display the correct, non-spoofable requesting origin**, considering:
    *   Opaque origins (should likely show top-level origin or block).
    *   Permission delegation (must show top-level origin).
    *   Initiation from different contexts (iframes, workers?).
    *   Check the origin passed to `url_formatter::FormatOriginForSecurityDisplay` in `BluetoothChooserAndroid.cc` (line ~60). Where is this `origin` variable sourced from, especially when delegation is involved? (VRP2.txt#10263).
*   **Permission Flow (`BluetoothScanningPromptController`, `BluetoothChooserContext`):** Analyze the permission granting flow for both scanning and device selection. Is consent securely obtained and tied to the correct *top-level* origin? How are delegated permissions handled? Can permissions be bypassed or escalated?
*   **Device Discovery Logic (`BluetoothDeviceChooserController`, `BluetoothDiscoverySession`):** How are discovered devices filtered based on API options (`filters`, `optionalServices`)? Can this process leak excessive information about nearby devices? Are discovery sessions properly managed and terminated?
*   **GATT Communication Security:** Analyze the handling of GATT operations (read, write, subscribe) in `device/bluetooth` and platform backends. Look for vulnerabilities related to data parsing, state management, and buffer handling.
*   **Platform Bluetooth Stack Interaction:** Analyze security assumptions and interactions with the underlying OS Bluetooth stack (BlueZ, WinRT, CoreBluetooth) via `device/bluetooth`.

## 4. Code Analysis
*   `WebBluetoothServiceImpl`: Browser-side implementation of the `blink.mojom.WebBluetoothService` Mojo interface. Handles API calls like `RequestDevice`, `RequestLEScan`. Likely determines the initiating/embedding origins.
*   `BluetoothDeviceChooserController`: Manages device discovery (`StartDiscoverySessionWithFilter`) and presentation logic for the chooser. Passes origin information to the UI controllers.
*   `BluetoothChooserBubbleController`: Manages the desktop chooser UI bubble. Needs verification of origin display logic (how it receives/formats the origin from the controller).
*   `BluetoothChooserAndroid`: Android chooser UI implementation.
    *   Uses `url_formatter::FormatOriginForSecurityDisplay(origin)` (line ~60) to display the origin. **The source of this `origin` variable needs scrutiny**, especially in delegation scenarios (VRP2.txt#10263). It should be derived from the top-level frame, not the initiating frame.
*   `BluetoothScanningPromptController`: Handles the separate permission prompt for scanning.
*   `BluetoothChooserContext` (extends `ChooserContextBase`): Manages permissions granted for specific devices via the chooser (`GrantObjectPermission`). Ensure grants are associated with the correct top-level origin.
*   `BluetoothAllowedDevicesMap`: Stores granted device permissions per origin.
*   `device/bluetooth/`: Platform abstraction layer. Key classes: `BluetoothAdapter`, `BluetoothDevice`, `BluetoothDiscoverySession`, `BluetoothGattConnection`, `BluetoothGattCharacteristic`, `BluetoothGattService`. Focus on state management and data handling during GATT operations.

## 5. Areas Requiring Further Investigation
*   **Android Origin Handling:** **Trace the source of the `origin` variable passed to `FormatOriginForSecurityDisplay` in `BluetoothChooserAndroid.cc`**. How is it determined by the caller (likely `BluetoothDeviceChooserController` or `WebBluetoothServiceImpl`)? Ensure the top-level frame's origin is retrieved and used when handling opaque origins or permission delegation.
*   **Permission Delegation Logic:** Verify that permission delegation via Permissions Policy is correctly checked and the *top-level origin* is used consistently throughout the Web Bluetooth flow (chooser display in `BluetoothChooserBubbleController` and `BluetoothChooserAndroid`, permission granting in `BluetoothChooserContext`).
*   **GATT Operation Security:** Fuzzing and code review of GATT communication handling in `device/bluetooth` and platform backends, focusing on characteristic read/write operations and descriptor handling.
*   **Discovery Filtering & Information Leaks:** Verify that device filtering during discovery doesn't leak more information than intended by the spec.
*   **Error Handling:** Analyze error paths during discovery, connection, and GATT communication.

## 6. Related VRP Reports
*   VRP: `40061374` / `40061373` / VRP2.txt#8904, #9771 (Chooser dialog origin spoofing for opaque origins - affects multiple device APIs)
*   VRP2.txt#10263 (Android Chooser dialog uses iframe origin instead of top-level with permission delegation)

## 7. Cross-References
*   [webusb.md](webusb.md)
*   [webserial.md](webserial.md)
*   [permissions.md](permissions.md)
*   [permissions_policy.md](permissions_policy.md)
*   [url_formatting.md](url_formatting.md) (Related to origin display)
