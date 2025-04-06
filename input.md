# Component: Input Handling (Mouse, Keyboard, Touch, etc.)

## 1. Component Focus
*   This page focuses on how Chromium handles user input from various sources, including mouse clicks/movement, keyboard events, touch events, pointer events, and related APIs.
*   Relevant areas include event dispatch (`EventHandler`), input processing (`InputHandler`), custom cursors, Pointer Lock API, EyeDropper API, and interactions with UI elements like autofill prompts and permission dialogs.
*   Key files: `content/browser/renderer_host/render_widget_host_view_aura.cc`, `third_party/blink/renderer/core/input/event_handler.cc`, `third_party/blink/renderer/modules/eyedropper/`, `third_party/blink/renderer/modules/pointer_lock/`.

## 2. Potential Logic Flaws & VRP Relevance
*   **Input/Interaction Hijacking (Clickjacking/Keyjacking):**
    *   Exploits where user input intended for one element is captured by another, often obscured element.
    *   **VRP Pattern (Custom Cursor Spoofing):** Large or misleading custom cursors overlaying sensitive UI elements or confusing the user about the true pointer position. Compromised renderers could bypass size limits. (VRP: `40057147`, `1381087`, `1376859`; VRP2.txt Line 14512, 11789, 15458, 13861).
    *   **VRP Pattern (Keyjacking):** Capturing keypresses intended for focused dialogs (e.g., PaymentRequest, Permissions, Downloads) when the dialog is obscured or focus is manipulated. (VRP: `1403539`, `1371215`; VRP2.txt Line 4303, 1478, 13544, 9286).
*   **Autofill Bypasses via Input Methods:** Using non-standard input methods or manipulating input state to bypass security checks for autofill.
    *   **VRP Pattern (EyeDropper):** Using the EyeDropper API to trigger autofill without required user interaction. (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt Line 825). See [eye_dropper.md](eye_dropper.md).
    *   **VRP Pattern (Pointer Lock):** Using Pointer Lock to bypass mouse movement requirements for autofill. (VRP: `40056870`; VRP2.txt Line 143). See [pointer_lock.md](pointer_lock.md).
    *   **VRP Pattern (Taps/Clicks):** Exploiting double-taps or precise click locations near/under prompts to trigger autofill. (VRP: `40060134`, `40058217`, `40056900`, VRP2.txt#1426679, VRP2.txt#1487440, VRP2.txt#9878, VRP2.txt#8036).
    *   **VRP Pattern (Keyboard Events):** Using specific keys (like Space) or sequences to bypass checks. (VRP: `40056936`). Physical keyboard accessory interaction (VRP2.txt#13367).
*   **Permission Prompt Bypasses:** Using input tricks to accept permission prompts without user awareness.
    *   **VRP Pattern (Tapjacking):** Using double-taps or obscured elements to trick users into accepting prompts (VRP2.txt#1478, #10088, #15112, #7863).
*   **State Confusion:** Exploiting timing issues or race conditions related to input event handling (e.g., moving elements between `mousedown` and prompt rendering - VRP: `40058217`, VRP2.txt#10877).

## 3. Further Analysis and Potential Issues
*   How is input event routing handled between different frames, processes, and UI elements (browser UI vs. web content)?
*   Are there edge cases in focus management (`FocusController`) that could be exploited?
*   How does input handling interact with accessibility features?
*   Analysis of input event timing and potential for side-channel leaks (VRP: `1315899` - Keystroke timing).
*   Review mitigations for clickjacking/keyjacking, especially around prompt delays (`InputEventActivationProtector`) and visibility checks. Are tap events adequately protected (Ref VRP2.txt#8036, #1478)?

## 4. Code Analysis
*   `EventHandler::HandleMousePressEvent`, `HandleMouseMoveEvent`, `HandleMouseReleaseEvent`, `HandleWheelEvent`, `HandleKeyboardEvent`
*   `RenderWidgetHostViewAura::Forward*Event*` methods
*   `InputHandlerProxy::HandleInputEvent`
*   `EventHandler::UpdateCursor` and custom cursor logic (size limits, fallback checks).
*   `PointerLockController`, `MouseLockDispatcher` (See [pointer_lock.md](pointer_lock.md)).
*   `EyeDropper` module implementation (See [eye_dropper.md](eye_dropper.md)).
*   Autofill popup interaction logic (`AutofillPopupViewNativeViews` methods like `HandleKeyPressEvent`, `OnMouseMoved`, `OnGestureEvent`). See [autofill_ui.md](autofill_ui.md).
*   Permission prompt event handling (`PermissionPromptBubbleView`, `PermissionRequestManager`). See [permissions.md](permissions.md).

## 5. Areas Requiring Further Investigation
*   Robustness of click/tap/keyjacking mitigations across all relevant UI prompts (Autofill, Permissions, Payments, Downloads, FedCM, etc.).
*   Interaction between custom cursors and sensitive browser UI elements (permissions, omnibox, etc.).
*   Potential for input event spoofing or manipulation via compromised renderers (related to IPC/Mojo security).
*   Timing attack possibilities related to input processing delays.
*   Examine the `StartDragging` Mojo interface mentioned in VRP2.txt#4 - how does it relate to standard input handling and potential escapes? Link to (`ipc.md` / `mojo.md`).

## 6. Related VRP Reports
*   VRP: `40057147` (Custom cursor overlay)
*   VRP: `1381087` (Custom cursor overlay)
*   VRP: `1376859` (Custom cursor overlay)
*   VRP: `1403539` (Keyjacking PaymentRequest)
*   VRP: `1371215` (Keyjacking Permissions)
*   VRP: `40065604`, `40063230`, `40058496` (Autofill bypass via EyeDropper)
*   VRP: `40056870` (Autofill bypass via PointerLock)
*   VRP: `40060134`, `40058217`, `40056900` (Autofill bypass via taps/clicks)
*   VRP: `40056936` (Autofill bypass via space key)
*   VRP: `1466230` (EyeDropper cursor position confusion)
*   VRP2.txt: #4 (Compromised renderer mouse control via `StartDragging`)
*   VRP2.txt: #1478, #8036, #10088, #15112, #7863 (Permission/Tapjacking)
*   VRP2.txt: #4303 (Keyjacking PaymentRequest)
*   VRP2.txt: #13544 (Keyjacking Permissions)
*   VRP2.txt: #9286 (Keyjacking Download UI)
*   VRP2.txt: #11789, #15458 (Custom Cursor overlay)
*   VRP2.txt: #10877 (Autofill timing/state issue)
*   VRP2.txt: #13367 (Physical keyboard accessory autofill bypass)
*   VRP2.txt: #1315899 (Keystroke timing leak)

*(Note: This page consolidates general input handling. Specific APIs like PointerLock, EyeDropper have dedicated pages. Autofill and Permissions pages also cover input-related bypasses specific to their UI.)*