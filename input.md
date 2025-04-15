# Component: Input Handling (Mouse, Keyboard, Touch, Cursors)

## 1. Component Focus
*   **Functionality:** Handles user input events from various sources (mouse, keyboard, touch, stylus), processes these events (hit testing, routing, dispatching), manages related UI states (focus, cursor style, pointer lock), and implements APIs that interact with input (Pointer Lock, EyeDropper, Custom Cursors).
*   **Key Logic:** Event capturing and routing (`RenderWidgetHostViewAura`, `InputRouterImpl`), Blink-side event handling (`EventHandler`, `PointerEventManager`, `TouchEventManager`, `KeyboardEventManager`), focus management (`FocusController`), cursor management (`WebCursor`, `CursorManager`), specific API implementations (`PointerLockController`, `EyeDropper`).
*   **Core Files:**
    *   `content/browser/renderer_host/input/`: Browser-side input routing logic (e.g., `input_router_impl.cc`).
    *   `content/browser/renderer_host/render_widget_host_view_aura.cc`/`.h`: Handles input events for Aura platform.
    *   `third_party/blink/renderer/core/input/`: Core Blink event handling (`event_handler.cc`, `keyboard_event_manager.cc`, `pointer_event_manager.cc`, `touch_event_manager.cc`).
    *   `third_party/blink/renderer/core/exported/web_input_event.*`: Event structures.
    *   `third_party/blink/renderer/core/page/focus_controller.cc`/`.h`: Focus management.
    *   `third_party/blink/renderer/core/css/cursor_data.*`: Custom cursor data.
    *   `content/common/cursors/webcursor.*`: Cursor representation and platform handling.
    *   `ui/events/`: Base event types and dispatching.
    *   `ui/views/widget/widget.cc`: Handles event dispatch to views.
    *   `ui/views/input_event_activation_protector.h/.cc`: Helper for preventing interaction bypasses.
    *   Pointer Lock: `third_party/blink/renderer/modules/pointer_lock/`, `content/browser/pointer_lock/`. See [pointer_lock.md](pointer_lock.md).
    *   EyeDropper: `third_party/blink/renderer/modules/eyedropper/`, `components/eye_dropper/`. See [eye_dropper.md](eye_dropper.md).

## 2. Potential Logic Flaws & VRP Relevance

Input handling is central to UI security. Flaws often involve bypassing interaction requirements, confusing user intent, or manipulating focus/cursor state.

*   **Interaction Requirement Bypass:** Using non-standard input methods, timing tricks, or focus manipulation to trigger actions (like Autofill, Permissions) without the intended level of user interaction or awareness.
    *   **VRP Pattern (Autofill - Input Method Abuse):** Frequently exploited via:
        *   *EyeDropper API:* Opening/closing chooser bypasses checks (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825, #13922). See [eye_dropper.md](eye_dropper.md).
        *   *Pointer Lock API:* Bypasses mouse movement checks (VRP: `40056870`; VRP2.txt#143). See [pointer_lock.md](pointer_lock.md).
        *   *Tap Events (Android):* Double-taps, taps near/under prompts, keyboard accessory taps bypass delays/checks (VRP: `40060134`, `40058217`, `40056900`; VRP2.txt#1426679, #1487440, #9878, #13367).
        *   *Keyboard Events:* Space key, potentially others (VRP: `40056936`).
    *   **VRP Pattern (Permission - Tapjacking):** Using double-taps or precisely timed taps to accept permission prompts while obscured or immediately after appearing (VRP2.txt#1478, #8036, #10088, #15112, #7863). See [permissions.md](permissions.md).
    *   **VRP Pattern (Timing/State Confusion):** Exploiting delays between input events (`mousedown`) and UI updates (prompt rendering) by moving elements to bypass interaction checks (VRP: `40058217`; VRP2.txt#10877). See [autofill.md](autofill.md).
    *   **VRP Pattern (Focus/Resize Abuse):** Freezing/resizing the browser window during prompt display to bypass interaction delays (VRP2.txt#10088, #13545).

*   **Input/Interaction Hijacking (Clickjacking/Keyjacking/Cursor Spoofing):** Intercepting or misdirecting user input.
    *   **VRP Pattern (Keyjacking):** Capturing keypresses (Tab, Enter) intended for focused but obscured dialogs (PaymentRequest, Permissions, Downloads UI). Often possible when the obscuring element doesn't steal focus or when input protection is missing/bypassed. (VRP: `1403539`, `1371215`; VRP2.txt#4303, #1478, #13544, #9287). See [payments.md](payments.md), [permissions.md](permissions.md), [downloads.md](downloads.md).
    *   **VRP Pattern (Custom Cursor Spoofing/Obscuring):**
        *   *Large Cursors:* Compromised renderers historically bypassed size limits (128x128), allowing cursors up to 1024x1024 to overlay browser UI (VRP: `40057147`; VRP2.txt#14512). Needs browser-side size enforcement.
        *   *Misleading Cursors:* Cursors visually misrepresenting the actual click point or overlaying sensitive UI like permission prompts (VRP: `1381087`, `1376859`; VRP2.txt#11789, #15458, #13861, #13795 - Autofill context). Checks (`EventHandler::CursorUpdatePendingOrNeeded`) aim to prevent overlaying outside the page, but might be bypassed.
    *   **VRP Pattern (Drag & Drop IPC):** Compromised renderer sending `StartDragging` Mojo IPC to gain global mouse control (VRP2.txt#4). Bypasses standard event handling. See [drag_and_drop.md](drag_and_drop.md), [mojo.md](mojo.md).

*   **Information Leaks via Input Timing:**
    *   **VRP Pattern (Keystroke Timing):** Precise timing analysis of keyboard event handling potentially leaking typed characters (VRP: `1315899`; VRP2.txt#4417).

## 3. Further Analysis and Potential Issues
*   **Input Protection Robustness:** How consistently is `InputEventActivationProtector` (or equivalent delays) used for sensitive UI actions (permission grants, autofill selection, payments, downloads)? Are tap events always covered? Are there bypasses related to focus changes, window resizing, or specific event sequences? (Related to VRP2.txt#1478, #4303, #10088, #13545).
*   **Custom Cursor Security:** Are renderer-side size limits (128x128) now enforced browser-side? Does the check preventing cursors >32x32 from rendering outside the web content area (`EventHandler::SetCursor`) have bypasses (e.g., via iframes VRP2.txt#13861)? Can custom cursors still obscure permission prompts (VRP2.txt#11789, #15458)?
*   **Focus Management (`FocusController`):** How does focus interact with modal dialogs, popups, iframes, Pointer Lock, and EyeDropper? Can focus be manipulated to misdirect input or bypass interaction requirements?
*   **Event Routing & Trust:** How are events routed between processes (Browser↔Renderer)? Can a compromised renderer spoof input events or their properties (coordinates, activation state)?
*   **Platform Differences:** Input handling (especially touch, keyboard accessory, cursor rendering) can vary significantly across platforms (Win, Mac, Linux, Android, CrOS). Test bypasses on all relevant platforms.
*   **Interaction with Accessibility Features:** How does input handling interact with screen readers, switches, or other assistive technologies? Are there unique security considerations?

## 4. Code Analysis
*   `EventHandler` (Blink): Core event handling logic (`HandleMouse*`, `HandleKeyboardEvent`, `HandleTouchEvent`). `SetCursor` applies custom cursors.
*   `PointerEventManager`, `TouchEventManager`, `KeyboardEventManager` (Blink): Manage state for specific input types.
*   `RenderWidgetHostViewAura::Forward*Event*`: Platform view handling input events, forwarding to `InputRouter`.
*   `InputRouterImpl` (`content/browser/renderer_host/input/`): Routes input events to the correct handler (renderer, browser).
*   `WebCursor`, `CursorManager`: Handling cursor types and platform rendering. Check size enforcement.
*   `FocusController` (Blink): Manages focus within a page.
*   `InputEventActivationProtector`: Time-based protection against rapid interactions. **Needs verification if used consistently on sensitive UI.**
*   Pointer Lock: `PointerLockController`, `MouseLockDispatcher`. See [pointer_lock.md](pointer_lock.md).
*   EyeDropper: `EyeDropper` module, `EyeDropperView`. See [eye_dropper.md](eye_dropper.md).
*   Autofill: `AutofillPopupView*`, `AutofillPopupControllerImpl`. `HandleKeyPressEvent`, `OnMouseMoved`, `OnGestureEvent`. See [autofill.md](autofill.md).
*   Permissions: `PermissionPrompt*View`. See [permissions.md](permissions.md).
*   Payments: `PaymentRequestSheetController`. See [payments.md](payments.md).

## 5. Areas Requiring Further Investigation
*   **Input Protection Audit:** Systematically audit all sensitive UI interactions (Autofill, Permissions, Payments, Downloads, FedCM) for consistent and robust use of interaction delays (`InputEventActivationProtector` or equivalent) against click/tap/keyjacking.
*   **Custom Cursor Enforcement:** Verify browser-side enforcement of cursor size limits. Test custom cursor interactions with sensitive UI elements (prompts, omnibox) for obscuring/spoofing potential.
*   **Focus Manipulation Attacks:** Investigate scenarios involving rapid focus changes, popups, or iframes to bypass interaction requirements or misdirect input.
*   **Compromised Renderer Input Spoofing:** Analyze IPC validation for input events. Can activation state, coordinates, or event types be spoofed?
*   **Android Tap/Keyboard Handling:** Specific review of Android event handling for tapjacking and keyboard accessory interaction bypasses.

## 6. Related VRP Reports
*   **Autofill Bypass:** VRP: `40065604` (EyeDropper), `40056870` (PointerLock), `40060134`/`40058217`/`40056900` (Taps/Clicks), `40056936` (Space Key); VRP2.txt#825/13922 (EyeDropper), #143 (PointerLock), #1426679/1487440/9878 (Taps), #13367 (Keyboard Accessory), #10877 (Timing/Move).
*   **Permission Bypass:** VRP2.txt#1478, #8036, #10088, #15112, #7863 (Tapjacking).
*   **Keyjacking:** VRP: `1403539` (Payments), `1371215` (Permissions); VRP2.txt#4303 (Payments), #1478/#13544 (Permissions), #9287 (Download UI).
*   **Custom Cursor Issues:** VRP: `40057147` (Large Cursor/Compromised Renderer), `1381087`/`1376859` (Overlay); VRP2.txt#11789/#15458 (Overlay Permissions), #13861 (General Overlay), #13795 (Overlay Autofill).
*   **EyeDropper Confusion:** VRP: `1466230`; VRP2.txt#12404.
*   **Drag&Drop IPC:** VRP2.txt#4 (Mouse control via Mojo).
*   **Timing Leaks:** VRP: `1315899`; VRP2.txt#4417 (Keystroke timing).

## 7. Cross-References
*   [autofill.md](autofill.md)
*   [permissions.md](permissions.md)
*   [payments.md](payments.md)
*   [downloads.md](downloads.md)
*   [eye_dropper.md](eye_dropper.md)
*   [pointer_lock.md](pointer_lock.md)
*   [drag_and_drop.md](drag_and_drop.md)
*   [mojo.md](mojo.md)