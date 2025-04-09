# Component: Permissions

## 1. Component Focus
*   **Functionality:** Manages permission requests, status queries, and storage for various web platform features (e.g., Geolocation, Notifications, Camera, Mic, MIDI, Sensors, Storage Access, etc.). Includes the core permission logic (`PermissionControllerImpl`), delegation mechanisms, UI prompts (standard prompts, Page Embedded Permission Control - PEPC), and persistence.
*   **Key Logic:** Request validation (`IsRequestAllowed`), status checking (`GetPermissionStatusFor*`), permission granting/revocation (via `PermissionControllerDelegate`), UI display logic (`PermissionPrompt`, `PermissionRequestManager`), Permission Policy integration, handling of different contexts (secure contexts, iframes, workers, Incognito).
*   **Core Files:**
    *   `content/browser/permissions/permission_controller_impl.cc` / `.h`
    *   `components/permissions/` (core logic, prompts, context checks - e.g., `permission_request_manager.cc`, `permission_context_base.cc`, `permission_uma_util.cc`)
    *   `chrome/browser/ui/views/permissions/` (Desktop UI views - including `permission_prompt_base_view.cc`, `embedded_permission_prompt.cc`, `embedded_permission_prompt_content_scrim_view.cc`)
    *   `chrome/browser/permissions/` (Chrome-specific permission logic/delegates)
    *   `third_party/blink/renderer/modules/permissions/` (Renderer-side API)
    *   `third_party/blink/public/mojom/permissions/permission.mojom`

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring/Spoofing:** Permission prompts being hidden or spoofed by other UI elements.
    *   **Mechanism (General):** `PermissionPromptBaseView` uses `PictureInPictureOcclusionTracker` via `occlusion_observation_.Observe(GetWidget())` to detect occlusion by always-on-top windows. The state is tracked in `occluded_by_picture_in_picture_` via `OnOcclusionStateChanged`, and `ShouldIgnoreButtonPressedEventHandling` uses this flag to block input when occluded.
    *   **Mechanism (PEPC Anchoring/Positioning):** Unlike standard bubbles, `EmbeddedPermissionPrompt` anchors to an intermediate `EmbeddedPermissionPromptContentScrimView` (`UpdateAnchor` in `EmbeddedPermissionPromptBaseView`). The final position is then calculated in `GetBubbleBounds` relative to the triggering element (`element_rect_`) and the web content container (`container_bounds`), attempting `kNearElement` placement first, then falling back to `kWindowMiddle`. The `kWindowMiddle` logic includes clamping to prevent the prompt rendering *above* the content area.
    *   **VRP Pattern (Overlaying Elements):** Prompts obscured by Picture-in-Picture windows (VRP: `342194497`, VRP2.txt#6928), extension popups/windows (VRP: `40058873`, VRP2.txt#7974, #12352), FedCM dialogs (VRP2.txt#13293 - potentially), custom cursors (VRP2.txt#11789, #15458). Requires robust occlusion checks. VRP #342194497 indicated the PiP occlusion mechanism might fail for PEPC prompts. This could be due to the indirect anchoring (via scrim) and complex bounds calculation (`GetBubbleBounds`) causing the `PictureInPictureOcclusionTracker` to misjudge the final prompt position relative to the PiP window. See [picture_in_picture.md](picture_in_picture.md), [extensions_api.md](extensions_api.md).
    *   **VRP Pattern (PEPC Positioning Bugs):** Page Embedded Permission Control (PEPC) element rendering outside its initiator window in small windows, potentially obscuring browser UI or appearing over other origins (VRP: `341663594`, VRP2.txt#12915). This might be related to failures in the `GetBubbleBounds` logic under certain conditions.
*   **Interaction Bypass (Clickjacking/Keyjacking/Tapjacking):** Tricking users into granting permissions without awareness.
    *   **VRP Pattern (Keyjacking):** Capturing keypresses (Tab, Enter) intended for an obscured permission prompt (VRP: `1371215`, VRP2.txt#1478, #13544). Requires robust input handling and delays (`InputEventActivationProtector`). Vulnerable if occlusion check fails.
    *   **VRP Pattern (Tapjacking):** Bypassing interaction delays using rapid taps/double-taps (VRP2.txt#1478, #8036, #10088, #15112). Explicit bypass for Permission Element tapjacking (VRP2.txt#7863). Requires consistent delay enforcement across input types. Android-specific tapjacking (VRP2.txt#8036).
    *   **VRP Pattern (Focus/Resize Abuse):** Freezing/resizing the browser window during prompt display to bypass interaction delays or hide the prompt entirely (VRP2.txt#10088, #13545).
*   **Incorrect Permission State/Logic:** Flaws in checking, granting, or revoking permissions.
    *   **VRP Pattern (State Confusion):** Race conditions or incorrect state management in `PermissionRequestManager` leading to dangling pointers or incorrect grant state after request cancellation/finalization (UAF VRP: `1424437`, VRP2.txt#1964).
    *   **VRP Pattern (Scope/Context Errors):** Incorrectly determining the requesting origin or context (e.g., iframe vs top-level), potentially leading to incorrect permission application. Check `VerifyContextOfCurrentDocument`.
    *   **VRP Pattern (Extension Permission Issues):** Extensions escalating privileges beyond granted permissions, potentially involving interaction with permission APIs or prompts (e.g., leaking tab info VRP: `1306167`). See [extensions_api.md](extensions_api.md).
    *   **VRP Pattern (Interaction with other APIs for Escalation):** While not direct permission bypasses, flaws in permission handling/state might be exploitable *in combination* with other APIs (e.g., Downloads API `onDeterminingFilename`, File System Access API) to achieve broader goals like unauthorized file access. See [downloads.md](downloads.md), [file_system_access.md](file_system_access.md). (Relevant to VRPs like `1428743`).
*   **Permission Policy Bypass:** Circumventing restrictions defined by the Permissions Policy (`//components/permissions_policy/`). See [permissions_policy.md](permissions_policy.md).
*   **Information Leaks:** APIs related to permissions leaking information cross-origin (e.g., status queries).

## 3. Further Analysis and Potential Issues
*   **PEPC Security:** Analyze the new Permission Element (<permission>). How robust are its visibility/occlusion checks (Intersection Observer V2 - VRP2.txt#7863 notes potential issues)? How does it handle focus and user interaction compared to traditional prompts? Are there unique spoofing/bypass vectors? (VRP: `341663594`, VRP2.txt#12915).
*   **Interaction Delay Enforcement:** Verify that the ~500ms delay (`InputEventActivationProtector`) preventing immediate acceptance is consistently applied to *all* input types (click, tap, keypress) across *all* prompt types (standard, PEPC) and platforms. VRPs show repeated bypasses.
*   **Occlusion Checks & Reliability (PEPC Focus):** While `PermissionPromptBaseView` implements PiP occlusion checks via `PictureInPictureOcclusionTracker`, VRP #342194497 suggests this might not be reliable for PEPC. The indirect anchoring to the `EmbeddedPermissionPromptContentScrimView` and the complex final bounds calculation in `GetBubbleBounds` might cause the tracker to misjudge the situation. Investigate the tracker logic (`PictureInPictureOcclusionTracker`) and the `EmbeddedPermissionPromptContentScrimView` implementation.
*   **State Management (`PermissionRequestManager`):** Audit the lifecycle of `PermissionRequest` objects, especially concerning cancellation, duplication (`duplicate_requests_`), and finalization (`RequestFinishedIncludingDuplicates`). Ensure no dangling pointers or state inconsistencies can arise (VRP: `1424437`).
*   **Context Verification:** How thoroughly does `VerifyContextOfCurrentDocument` check the requesting context (secure context, origin, potentially active features/flags)?
*   **Delegate Implementation:** The core permission logic resides in the `PermissionControllerDelegate`. Need to analyze specific implementations (e.g., `chrome/browser/permissions/permission_manager_impl.cc`) for correctness in granting, denying, and revoking permissions, especially for sensitive ones like Camera/Mic.
*   **Incognito/Guest Handling:** How are permission states isolated between normal, Incognito, and Guest profiles?
*   **Implicit Permission Granting (Drag/Drop, File System Access):** Contrast with standard prompts, features like drag-and-drop (`PrepareDropDataForChildProcess`) and the File System Access API grant file read access implicitly based on user action. These mechanisms rely on `IsolatedContext` for scoping, but the core permission decision hinges on the integrity and validation of the data (e.g., `DropData`, FSA handles) received by the browser process *at the time of the action*, rather than explicit user confirmation via a prompt. Analyze potential timing windows or data manipulation vectors *before* the final permission grant occurs within these flows. Ensure the source validation (e.g., renderer taint, Files App origin check) is robust. See [drag_and_drop.md](drag_and_drop.md), [file_system_access.md](file_system_access.md).

## 4. Code Analysis
*   `PermissionControllerImpl`: Core browser-side controller (`RequestPermissions`, `GetPermissionStatus*`, `Subscribe/Unsubscribe`). Uses `PermissionControllerDelegate`.
*   `PermissionRequestManager` (`components/permissions/`): Manages the queue and UI display of permission requests. Handles coalescing, prioritization, and finishing requests. Vulnerable to state confusion (VRP: `1424437`).
*   `PermissionPrompt` (`components/permissions/`): Interface for permission UI. Implementations include bubble views, PEPC.
*   `PermissionPromptBaseView` (`chrome/browser/ui/views/permissions/`): Base class for desktop permission bubbles/prompts. Contains PiP occlusion check logic (`StartTrackingPictureInPictureOcclusion`, `OnOcclusionStateChanged`, `ShouldIgnoreButtonPressedEventHandling`, `occluded_by_picture_in_picture_`). Calculations for final prompt position happen in `GetBubbleBounds`.
*   `EmbeddedPermissionPrompt` (`chrome/browser/ui/views/permissions/`): Controller for PEPC views. Manages the specific view types and the content scrim. Calls `UpdateAnchor` on the view, passing the scrim widget.
*   `EmbeddedPermissionPrompt...View` (`chrome/browser/ui/views/permissions/`): Specific PEPC prompt views (Ask, PreviouslyGranted, etc.), inheriting from `PermissionPromptBaseView`.
*   `EmbeddedPermissionPromptContentScrimView` (`chrome/browser/ui/views/permissions/`): Implements the scrim layer used as the anchor for PEPC prompts.
*   `InputEventActivationProtector` (`ui/views/bubble/bubble_dialog_delegate_view.cc`?): Helper class potentially used for interaction delays. Verify its usage and effectiveness.
*   `PermissionContextBase` (`components/permissions/`): Base class for context-specific permission logic (e.g., `GeolocationPermissionContextImpl`). Contains context verification logic.
*   `PermissionControllerDelegate` (`chrome/browser/permissions/permission_manager_impl.cc`): Chrome's implementation handling actual permission storage and decisions.
*   `PictureInPictureOcclusionTracker` (`chrome/browser/picture_in_picture/`): Tracks always-on-top windows and determines occlusion state. Needs investigation regarding how it handles widgets anchored indirectly (like PEPC via scrim).

## 5. Areas Requiring Further Investigation
*   **PEPC Occlusion Bypass:** Why might `OnOcclusionStateChanged` not fire reliably for embedded PEPC prompts compared to bubble prompts (VRP #342194497)? Investigate the interaction between the `PictureInPictureOcclusionTracker`, the `EmbeddedPermissionPromptContentScrimView`, and the final calculated `GetBubbleBounds` position.
*   **PEPC Vulnerabilities:** Further analyze the Permission Element - interaction bypasses (tapjacking VRP2.txt#7863), UI spoofing, visibility check robustness (Intersection Observer v2 issues?), positioning bugs (VRP: `341663594`).
*   **Interaction Delay Consistency:** Audit all code paths leading to permission grant/deny in UI views to ensure the interaction delay protector is consistently applied for taps, clicks, and relevant key presses (Enter).
*   **`PermissionRequestManager` State:** Perform a detailed state machine analysis of `PermissionRequestManager`, focusing on request cancellation, duplication, and finishing logic to prevent UAF/state confusion bugs (VRP: `1424437`).
*   **Cross-Component Obscuring:** Test interactions between permission prompts and *all* potential overlaying elements (PiP, FedCM, Extensions, Side Panel, System Notifications, Custom Cursors).
*   **Permission Policy Interaction:** How does Permissions Policy interact with standard permission prompts and PEPC?
*   **Interaction with Other APIs:** Explore how permission state/UI interactions could be combined with other APIs (Downloads, FSA, etc.) in exploit chains.

## 6. Related VRP Reports
*   **UI Obscuring:** VRP: `342194497` (PiP), `40058873` (Extension Popup)
*   **Interaction Bypass (Keyjacking):** VRP: `1371215`; VRP2.txt#1478, #13544
*   **Interaction Bypass (Tapjacking):** VRP2.txt#1478, #8036, #10088, #15112, #7863 (PEPC)
*   **Interaction Bypass (Focus/Resize):** VRP2.txt#10088, #13545
*   **State Confusion:** VRP: `1424437` (PermissionRequestManager UAF)
*   **PEPC Issues:** VRP: `341663594` (Positioning); VRP2.txt#7863 (Tapjacking/Overlay)
*   **Extension Interaction:** VRP: `1306167` (Info leak potentially enabling permission abuse), VRP2.txt#12352 (Extension obscuring prompt)

*(Note: This component is tightly coupled with UI elements ([autofill.md](autofill.md), [input.md](input.md)) and specific features like Geolocation, Media Capture etc.)*
*(See also: [extensions_api.md](extensions_api.md), [downloads.md](downloads.md), [file_system_access.md](file_system_access.md))*
