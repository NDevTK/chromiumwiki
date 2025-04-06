# Component: Web Bluetooth

## 1. Component Focus
*   **Functionality:** Implements the Web Bluetooth API ([Spec](https://webbluetoothcg.github.io/web-bluetooth/)), allowing web pages to discover and communicate with nearby Bluetooth Low Energy (BLE) devices after user consent via a chooser dialog.
*   **Key Logic:** Handling API calls (`requestDevice`, `getAvailability`, etc.), managing device discovery (`BluetoothDeviceChooserController`), displaying the chooser UI (`BluetoothChooserBubbleController`), handling permissions (`BluetoothScanningPromptController`, `ChooserContextBase`), interacting with the platform's Bluetooth stack (`device/bluetooth`).
*   **Core Files:**
    *   `content/browser/bluetooth/` (Core browser-side logic, e.g., `web_bluetooth_service_impl.cc`)
    *   `components/permissions/contexts/bluetooth_chooser_context.cc` (Permission management)
    *   `chrome/browser/ui/bluetooth/` (UI components, e.g., `bluetooth_chooser_bubble_controller.cc`)
    *   `device/bluetooth/` (Platform abstraction layer)
    *   `third_party/blink/renderer/modules/bluetooth/` (Renderer-side API implementation)

## 2. Potential Logic Flaws & VRP Relevance
*   **Chooser UI Spoofing/Origin Confusion:** The Bluetooth device chooser dialog failing to correctly display the requesting origin, especially with opaque origins, or allowing the dialog to overlay unrelated content.
    *   **VRP Pattern (Chooser Origin Spoofing):** Dialog showing empty origin for opaque initiators (sandboxed iframes, data URLs) (VRP: `40061374`, `40061373`; VRP2.txt#8904 - applies to multiple choosers including Bluetooth).
*   **Permission Bypass:** Circumventing user consent requirements for scanning or connecting to devices.
*   **Information Leaks:** Leaking sensitive information about nearby devices or the user's Bluetooth configuration.
*   **Device Interaction Flaws:** Vulnerabilities in handling communication with BLE devices (GATT services/characteristics).
*   **Race Conditions:** Issues during device discovery, connection establishment, or permission handling.

## 3. Further Analysis and Potential Issues
*   **Origin Display in Chooser:** Verify that `BluetoothChooserBubbleController` and related UI components always display the correct requesting origin, especially for opaque origins (VRP: `40061374`). How is the origin passed from the renderer and verified?
*   **Permission Flow (`BluetoothScanningPromptController`, `ChooserContextBase`):** Analyze the permission granting flow. Is consent securely obtained and managed? Can permissions be bypassed or escalated?
*   **Device Discovery Logic:** How are discovered devices filtered and presented to the user? Can this process leak information or be manipulated?
*   **Platform Bluetooth Stack Interaction:** Analyze the security assumptions and interactions with the underlying OS Bluetooth stack via `device/bluetooth`.

## 4. Code Analysis
*   `WebBluetoothServiceImpl`: Browser-side implementation of the Mojo interface. Handles API calls like `RequestDevice`.
*   `BluetoothDeviceChooserController`: Manages the logic for device discovery and presentation in the chooser.
*   `BluetoothChooserBubbleController`: Manages the chooser UI bubble. Check origin display logic.
*   `BluetoothScanningPromptController`: Handles the permission prompt for scanning.
*   `BluetoothChooserContext` (extends `ChooserContextBase`): Manages permissions granted via the chooser.
*   `content/browser/bluetooth/bluetooth_allowed_devices_map.cc`: Stores granted device permissions.

## 5. Areas Requiring Further Investigation
*   **Opaque Origin Handling:** Thoroughly test chooser UI display when initiated from various opaque origins (data:, blob:, srcdoc, sandboxed iframes).
*   **Permission Lifetime & Revocation:** How are permissions managed over time and revoked?
*   **Error Handling:** Analyze error paths during discovery, connection, and communication.

## 6. Related VRP Reports
*   VRP: `40061374` / `40061373` / VRP2.txt#8904 (Chooser dialog origin spoofing for opaque origins - affects Bluetooth, USB, Serial)

*(See also [webusb.md](webusb.md), [webserial.md](webserial.md), [permissions.md](permissions.md))*
