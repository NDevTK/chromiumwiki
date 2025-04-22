# Component: Permissions

## 1. Component Focus
*   **Functionality:** Manages permission requests, status queries, and storage for various web platform features (e.g., Geolocation, Notifications, Camera, Mic, MIDI, Sensors, Storage Access, etc.). Includes the core permission logic (`PermissionControllerImpl`), delegation mechanisms, UI prompts (standard prompts, Page Embedded Permission Control - PEPC), and persistence.
*   **Key Logic:** Request validation (`IsRequestAllowed`), status checking (`GetPermissionStatusFor*`), permission granting/revocation (via `PermissionControllerDelegate`), UI display logic (`PermissionPrompt`, `PermissionRequestManager`), Permission Policy integration, handling of different contexts (secure contexts, iframes, workers, Incognito).
*   **Core Files:**
    *   `content/browser/permissions/permission_controller_impl.cc` / `.h`
    *   `components/permissions/` (core logic, prompts, context checks - e.g., `permission_request_manager.cc`, `permission_context_base.cc`, `permission_uma_util.cc`)
    *   `chrome/browser/ui/views/permissions/` (Desktop UI views - including `permission_prompt_desktop.h`, `permission_prompt_bubble_base_view.h/.cc`, `permission_prompt_base_view.h/.cc`, `embedded_permission_prompt.h/.cc`, `embedded_permission_prompt_content_scrim_view.cc`)
    *   `chrome/browser/permissions/` (Chrome-specific permission logic/delegates)
    *   `ui/views/input_event_activation_protector.h/.cc`: Implements time-based input protection.
    *   `third_party/blink/renderer/modules/permissions/` (Renderer-side API)
    *   `third_party/blink/public/mojom/permissions/permission.mojom`

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring / Spoofing / Clickjacking:** Permission prompts being hidden or spoofed by other UI elements. (See README Tip #1 - UI Security)
    *   **Mechanism (General Occlusion Check):** `PermissionPromptBaseView` uses `PictureInPictureOcclusionTracker` to detect occlusion by always-on-top windows. `ShouldIgnoreButtonPressedEventHandling` uses this state (`occluded_by_picture_in_picture_`) to block input when occluded by PiP. This does *not* protect against occlusion by other non-always-on-top UI.
    *   **Mechanism (PEPC Anchoring/Positioning):** Unlike standard bubbles, `EmbeddedPermissionPrompt` anchors to an intermediate `EmbeddedPermissionPromptContentScrimView`. Final position uses `GetBubbleBounds`.
    *   **Mechanism (Showing Inactive):** `PermissionPromptBubbleBaseView::ShowWidget()` calls `ShowInactive()` if the parent browser window is not active. It also uses `set_close_on_deactivate(false)` (line 66). This allows the prompt to remain open and potentially interactable even when obscured by other UI (like FedCM) that doesn't steal focus.
    *   **VRP Pattern (Overlaying Elements):** Prompts obscured by Picture-in-Picture windows (VRP: `342194497`, VRP2.txt#6928), extension popups/windows (VRP: `40058873`, VRP2.txt#7974, #12352), FedCM dialogs (VRP2.txt#13293, #7963), Permission Prompts (VRP2.txt#6928), custom cursors (VRP2.txt#11789, #15458). Requires robust occlusion checks and input protection. VRP #342194497 indicated the PiP occlusion mechanism might fail for PEPC prompts. (See README Tip #1 - Dialog/Prompt Obscuring)
    *   **VRP Pattern (PEPC Positioning Bugs):** Page Embedded Permission Control (PEPC) element rendering outside its initiator window in small windows, potentially obscuring browser UI or appearing over other origins (VRP: `341663594`, VRP2.txt#12915). Related to `GetBubbleBounds` complexity. (See README Tip #1 - Dialog/Prompt Spoofing)
*   **Interaction Bypass (Clickjacking/Keyjacking/Tapjacking):** Tricking users into granting permissions without awareness. (See README Tip #1 - Input/Interaction Hijacking)
    *   **Mechanism:** The combination of `ShowInactive()` and `set_close_on_deactivate(false)` in `PermissionPromptBubbleBaseView`, coupled with **limited input protection** in the base class `PermissionPromptBaseView`. `PermissionPromptBaseView::FilterUnintenedEventsAndRunCallbacks` checks for PiP occlusion and uses `IsPossiblyUnintendedInteraction` (a basic dialog timing check based on `GetDoubleClickInterval`), but **lacks general input protection** (like `InputEventActivationProtector`) against interaction when obscured by non-PiP, non-focus-stealing UI (e.g., FedCM, other permission prompts shown inactive). This allows interaction bypasses. Contrast with FedCM UI which *does* use `InputEventActivationProtector`.
    *   **VRP Pattern (Keyjacking):** Capturing keypresses (Tab, Enter) intended for an obscured permission prompt (VRP: `1371215`; VRP2.txt#1478, #13544, #6928, #7963). Possible due to missing general input protection. (See README Tip #1 - Keyjacking)
    *   **VRP Pattern (Tapjacking):** Bypassing interaction delays using rapid taps/double-taps (VRP2.txt#1478, #8036, #10088, #15112). Explicit bypass for Permission Element tapjacking (VRP2.txt#7863). Requires consistent delay enforcement across input types. Android-specific tapjacking (VRP2.txt#8036). (See README Tip #1 - Tapjacking)
    *   **VRP Pattern (Focus/Resize Abuse):** Freezing/resizing the browser window during prompt display to bypass interaction delays or hide the prompt entirely (VRP2.txt#10088, #13545). (See README Tip #1 - Focus/Resize Abuse)
*   **Incorrect Permission State/Logic:** Flaws in checking, granting, or revoking permissions.
    *   **VRP Pattern (State Confusion):** Race conditions or incorrect state management in `PermissionRequestManager` leading to dangling pointers or incorrect grant state after request cancellation/finalization (UAF VRP: `1424437`, VRP2.txt#1964). (See README Tip #4 - State Confusion/Memory Safety)
    *   **VRP Pattern (Scope/Context Errors):** Incorrectly determining the requesting origin or context (e.g., iframe vs top-level), potentially leading to incorrect permission application. Check `VerifyContextOfCurrentDocument`.
    *   **VRP Pattern (Extension Permission Issues):** Extensions escalating privileges beyond granted permissions, potentially involving interaction with permission APIs or prompts (e.g., leaking tab info VRP: `1306167`). See [extension_security.md](extension_security.md). (See README Tip #2 - Extension & DevTools Security)
    *   **VRP Pattern (Interaction with other APIs for Escalation):** While not direct permission bypasses, flaws in permission handling/state might be exploitable *in combination* with other APIs (e.g., Downloads API `onDeterminingFilename`, File System Access API) to achieve broader goals like unauthorized file access. See [downloads.md](downloads.md), [file_system_access.md](file_system_access.md). (Relevant to VRPs like `1428743`). (See README Tip #5, Tip #9)
*   **Permission Policy Bypass:** Circumventing restrictions defined by the Permissions Policy (`//components/permissions_policy/`). See [permissions_policy.md](permissions_policy.md). (See README Tip #5 - Policy Bypass)
*   **Information Leaks:** APIs related to permissions leaking information cross-origin (e.g., status queries). (See README Tip #5 - Timing/Side-Channel Leaks)

## 3. Further Analysis and Potential Issues
*   **Input Protection:** **Crucially, investigate adding robust input protection (e.g., `InputEventActivationProtector`) to permission prompt views (`PermissionPromptBubbleBaseView`, `EmbeddedPermissionPrompt...View`)**. The current protection (`IsPossiblyUnintendedInteraction` for timing, PiP occlusion check) is insufficient against overlays like FedCM (Related to VRP: `339481295`, `342194497`). (See README Tip #1 - Input/Interaction Hijacking)
*   **Occlusion Checks & Reliability (PEPC Focus):** While `PermissionPromptBaseView` implements PiP occlusion checks, VRP #342194497 suggests this might not be reliable for PEPC. Investigate the `PictureInPictureOcclusionTracker`, `EmbeddedPermissionPromptContentScrimView`, and `GetBubbleBounds` interaction. Does the tracker correctly handle widgets anchored indirectly? (See README Tip #1 - Dialog/Prompt Obscuring)
*   **PEPC Security:** Analyze the new Permission Element (<permission>). How robust are its visibility/occlusion checks (Intersection Observer V2 - VRP2.txt#7863 notes potential issues)? How does it handle focus and user interaction compared to traditional prompts? Are there unique spoofing/bypass vectors? (VRP: `341663594`, VRP2.txt#12915). (See README Tip #1)
*   **Interaction Delay Enforcement:** Verify that the delay from `IsPossiblyUnintendedInteraction` is sufficient and consistently applied across platforms and prompt types. Analyze `InputEventActivationProtector` for comparison. (See README Tip #1)
*   **State Management (`PermissionRequestManager`):** Audit the lifecycle of `PermissionRequest` objects, especially concerning cancellation, duplication (`duplicate_requests_`), and finalization (`RequestFinishedIncludingDuplicates`). Ensure no dangling pointers or state inconsistencies can arise (VRP: `1424437`). (See README Tip #4 - State Confusion/Memory Safety)
*   **Context Verification:** How thoroughly does `VerifyContextOfCurrentDocument` check the requesting context (secure context, origin, potentially active features/flags)?
*   **Delegate Implementation:** The core permission logic resides in the `PermissionControllerDelegate`. Need to analyze specific implementations (e.g., `chrome/browser/permissions/permission_manager_impl.cc`) for correctness in granting, denying, and revoking permissions, especially for sensitive ones like Camera/Mic.
*   **Incognito/Guest Handling:** How are permission states isolated between normal, Incognito, and Guest profiles?
*   **Implicit Permission Granting (Drag/Drop, File System Access):** Analyze potential timing windows or data manipulation vectors *before* the final permission grant occurs within these flows. Ensure source validation is robust. See [drag_and_drop.md](drag_and_drop.md), [file_system_access.md](file_system_access.md). (See README Tip #9 - Data Transfer Boundaries)

## 4. Code Analysis
*   `PermissionControllerImpl`: Core browser-side controller (`RequestPermissions`, `GetPermissionStatus*`, `Subscribe/Unsubscribe`). Uses `PermissionControllerDelegate`.
*   `PermissionRequestManager` (`components/permissions/`): Manages the queue and UI display of permission requests. Handles coalescing, prioritization, and finishing requests. Vulnerable to state confusion (VRP: `1424437`).
*   `PermissionPrompt` (`components/permissions/`): Interface for permission UI. Implementations include bubble views, PEPC.
*   `PermissionPromptDesktop` (`chrome/browser/ui/views/permissions/`): Base class for desktop prompts. Doesn't show UI directly.
*   `PermissionPromptBaseView` (`chrome/browser/ui/views/permissions/`): Base class for desktop prompt views. Implements PiP occlusion check logic (`StartTrackingPictureInPictureOcclusion`, `OnOcclusionStateChanged`, `ShouldIgnoreButtonPressedEventHandling`). Button clicks call `FilterUnintenedEventsAndRunCallbacks`. Checks `IsPossiblyUnintendedInteraction`.
*   `PermissionPromptBubbleBaseView` (`chrome/browser/ui/views/permissions/`): Base class for bubble UI view. Inherits from `PermissionPromptBaseView` and `views::BubbleDialogDelegateView`. Uses `ShowInactive()` if parent browser inactive, uses `set_close_on_deactivate(false)`. **Lacks specific input protection mechanism like `InputEventActivationProtector`.** Calls `FilterUnintenedEventsAndRunCallbacks` for button actions.
*   `EmbeddedPermissionPrompt` (`chrome/browser/ui/views/permissions/`): Controller for PEPC views. Manages the specific view types and the content scrim. Calls `UpdateAnchor` on the view, passing the scrim widget.
*   `EmbeddedPermissionPrompt...View` (`chrome/browser/ui/views/permissions/`): Specific PEPC prompt views (Ask, PreviouslyGranted, etc.), inheriting from `PermissionPromptBubbleBaseView`.
*   `EmbeddedPermissionPromptContentScrimView` (`chrome/browser/ui/views/permissions/`): Implements the scrim layer used as the anchor for PEPC prompts.
*   `InputEventActivationProtector` (`ui/views/input_event_activation_protector.cc`): Helper class providing time-based input protection after visibility change or window movement. Blocks events within double-click interval of protection timestamp or previous event. **Not used by `PermissionPromptBubbleBaseView`.**
*   `PermissionContextBase` (`components/permissions/`): Base class for context-specific permission logic (e.g., `GeolocationPermissionContextImpl`). Contains context verification logic.
*   `PermissionControllerDelegate` (`chrome/browser/permissions/permission_manager_impl.cc`): Chrome's implementation handling actual permission storage and decisions.
*   `PictureInPictureOcclusionTracker` (`chrome/browser/picture_in_picture/`): Tracks always-on-top windows and determines occlusion state. Needs investigation regarding how it handles widgets anchored indirectly (like PEPC via scrim).

## 5. Areas Requiring Further Investigation
*   **Add Input Protection:** **Prioritize investigating the feasibility and impact of adding `InputEventActivationProtector` or similar mechanisms to `PermissionPromptBubbleBaseView` and related UI.** (See README Tip #1 - Input/Interaction Hijacking)
*   **PEPC Occlusion Bypass:** Why might `OnOcclusionStateChanged` not fire reliably for embedded PEPC prompts compared to bubble prompts (VRP #342194497)? Investigate the interaction between the `PictureInPictureOcclusionTracker`, the `EmbeddedPermissionPromptContentScrimView`, and the final calculated `GetBubbleBounds` position. (See README Tip #1 - Dialog/Prompt Obscuring)
*   **PEPC Vulnerabilities:** Further analyze the Permission Element - interaction bypasses (tapjacking VRP2.txt#7863), UI spoofing, visibility check robustness (Intersection Observer v2 issues?), positioning bugs (VRP: `341663594`). (See README Tip #1)
*   **Interaction Delay Consistency:** Audit all code paths leading to permission grant/deny in UI views to ensure the initial timing delay protector (`IsPossiblyUnintendedInteraction`) is consistently applied and sufficient. (See README Tip #1)
*   **`PermissionRequestManager` State:** Perform a detailed state machine analysis of `PermissionRequestManager`, focusing on request cancellation, duplication, and finishing logic to prevent UAF/state confusion bugs (VRP: `1424437`). (See README Tip #4 - State Confusion/Memory Safety)
*   **Cross-Component Obscuring:** Test interactions between permission prompts and *all* potential overlaying elements (PiP, FedCM, Extensions, Side Panel, System Notifications, Custom Cursors). (See README Tip #1 - Dialog/Prompt Obscuring)
*   **Permission Policy Interaction:** How does Permissions Policy interact with standard permission prompts and PEPC? (See README Tip #5 - Policy Bypass)
*   **Interaction with Other APIs:** Explore how permission state/UI interactions could be combined with other APIs (Downloads, FSA, etc.) in exploit chains. (See README Tip #5, Tip #9)

## 6. Related VRP Reports
*   **UI Obscuring:** VRP: `342194497` (PiP), `40058873` (Extension Popup); VRP2.txt#6928 (Perm Prompt on Perm Prompt), #7963 (FedCM on Autofill - related pattern)
*   **Interaction Bypass (Keyjacking):** VRP: `1371215`; VRP2.txt#1478, #13544, #6928, #7963 (Related pattern)
*   **Interaction Bypass (Tapjacking):** VRP2.txt#1478, #8036, #10088, #15112, #7863 (PEPC)
*   **Interaction Bypass (Focus/Resize):** VRP2.txt#10088, #13545
*   **State Confusion:** VRP: `1424437` (PermissionRequestManager UAF)
*   **PEPC Issues:** VRP: `341663594` (Positioning); VRP2.txt#7863 (Tapjacking/Overlay)
*   **Extension Interaction:** VRP: `1306167` (Info leak potentially enabling permission abuse), VRP2.txt#12352 (Extension obscuring prompt)

## 7. Cross-References
*   [autofill.md](autofill.md)
*   [input.md](input.md)
*   [fedcm.md](fedcm.md)
*   [picture_in_picture.md](picture_in_picture.md)
*   [extension_security.md](extension_security.md)
*   [downloads.md](downloads.md)
*   [file_system_access.md](file_system_access.md)
*   [permissions_policy.md](permissions_policy.md)
*   [drag_and_drop.md](drag_and_drop.md)
