# Component: Autofill

## 1. Component Focus
*   **Functionality:** Provides automatic filling of forms (addresses, credit cards, passwords) based on previously saved user data. Includes UI elements like dropdown suggestions (Autofill popup) and explicit prompts (e.g., card unmasking, save prompts).
*   **Key Logic:** Detecting fillable forms (renderer), sending form data to browser, parsing forms (`FormStructure`), querying saved data (`AutofillTable`, `PasswordFormStore`), ranking suggestions, displaying UI (`AutofillPopupControllerImpl`, various Views), handling user interaction, sending fill actions back to renderer, managing data saving/updating. Cross-origin iframe interactions are complex. Password Autofill interacts closely with the Password Manager.
*   **Core Files:**
    *   `components/autofill/content/renderer/autofill_agent.cc`: Renderer-side logic, interacts with Blink forms, sends IPC to browser.
    *   `components/autofill/content/browser/content_autofill_driver.cc`: Browser-side driver per frame, receives IPC, "lifts" data, interacts with manager.
    *   `components/autofill/core/browser/foundations/browser_autofill_manager.cc`: Core browser-side logic per frame, manages `FormStructure` cache, generates suggestions, triggers filling. **Note:** May delegate specific tasks (like payments, addresses) to helper classes, but remains the central manager.
    *   `components/autofill/core/browser/`: Common core logic (e.g., `form_structure.cc`, `field_types.h`, data models).
    *   `components/autofill/core/common/`: Shared types and constants.
    *   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc`: Manages the Autofill popup UI lifecycle and interactions.
    *   `chrome/browser/ui/autofill/autofill_external_delegate.*`: Interface between core logic and UI controllers.
    *   `chrome/browser/ui/views/autofill/`: Desktop UI views (e.g., `popup/autofill_popup_view_views.cc`, `payments/virtual_card_enroll_bubble_views.cc`). Check current names.
    *   `chrome/browser/autofill/android/` (`java/`): Android UI implementations (e.g., `AutofillPopupBridge`, keyboard accessory).
    *   `components/password_manager/core/browser/`: Password Autofill logic. ([password_manager.md](password_manager.md))
    *   `third_party/blink/public/web/web_autofill_client.h`: Blink interface implemented by `AutofillAgent`.
    *   `third_party/blink/renderer/core/html/forms/html_form_element.cc`, `html_input_element.cc`, etc.: Blink form element logic.

## 2. Potential Logic Flaws & VRP Relevance
Autofill is a frequent target due to the sensitivity of the data it handles (addresses, credit cards, names, emails, phone numbers). Common vulnerabilities involve bypassing user interaction requirements, UI obscuring/spoofing, and cross-origin issues.

*   **Interaction Requirement Bypass (High Likelihood, Medium-High VRP):** Exploiting non-standard input methods or timing issues to trigger autofill suggestions or fills without sufficient user intent (e.g., intentional mouse movement over the suggestion, keyboard selection, or click).
    *   **VRP Pattern (Input Method Abuse):** Using APIs or UI elements to simulate interaction or bypass checks.
        *   *EyeDropper API:* Repeatedly reported for bypassing interaction requirements. (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825, #13922). See [eye_dropper.md](eye_dropper.md).
        *   *Tap Events (Android):* Double-taps, taps near/under cursor, taps on keyboard accessory bypassing delays or checks. (VRP: `40060134`, `40058217`, `40056900`; VRP2.txt#1426679, #1487440, #9878, #13367 - keyboard accessory).
        *   *Space Key Input:* Using space key to trigger fill. (VRP: `40056936`).
        *   *Pointer Lock:* Abusing pointer lock to bypass interaction checks. (VRP: `40056870`). See [pointer_lock.md](pointer_lock.md).
        *   *Custom CSS Cursors:* Using large cursors to obscure prompts while interacting. (VRP2.txt#13795). See [input.md](input.md).
    *   **VRP Pattern (Timing/State Issues):** Exploiting delays between user action (e.g., `mousedown`) and prompt rendering/selection.
        *   *Moving Input Field:* Moving the input field after `mousedown` but before the popup appears, causing the popup to render under the (now stationary) cursor, allowing a second click to fill without intentional mouse movement over the suggestion. (VRP: `40058217`, `40056900`; VRP2.txt#10877).
        *   *Preview Text Leaks:* Historically, properties like `scrollWidth` or style calculations (`::first-line`, `@font-face`) on preview text could leak information about the suggested value before selection. (VRP: `1035058`, `1035063`, `1013882`, `951487`, `916838`; VRP2.txt#4826, #6190, #6157). `scrollTop` leak on `<select>` (VRP: `1250850`). Side-channel leaks via preview timing (VRP2.txt#14122, #14026). These specific leaks are likely fixed but highlight the sensitivity of preview state.

*   **UI Obscuring / Spoofing / Clickjacking (High Likelihood, Medium-High VRP):** Autofill popups or prompts obscuring or being obscured by other UI, leading to unintentional data filling/saving or bypassing user confirmation.
    *   **VRP Pattern (Other UI Obscures Autofill):** Autofill popups/prompts rendered underneath other browser UI elements. Critically, if the obscuring UI doesn't steal focus (e.g., uses `ShowInactive`), the underlying Autofill UI remains interactable via keyboard. **Autofill UI historically lacked robust input protection (e.g., `InputEventActivationProtector`) against this.**
        *   *Picture-in-Picture (PiP):* Video or Document PiP windows obscuring Autofill. (VRP: `40058582`; VRP2.txt#5228). See [picture_in_picture.md](picture_in_picture.md).
        *   *FedCM:* FedCM bubble dialog obscuring Autofill. (VRP: `339481295`, `340893685`; VRP2.txt#7963). See [fedcm.md](fedcm.md).
        *   *Permissions:* Permission prompts (PEPC) obscuring Autofill. (Related pattern: VRP2.txt#6928 where PiP obscures PEPC, similar interaction risk exists). See [permissions.md](permissions.md).
    *   **VRP Pattern (Visibility Check Bypass):** Tricking Autofill into showing suggestions for fields that are not geometrically visible (off-screen, zero opacity, clipped, covered by same-origin elements). Renderer-side checks (`AutofillAgent`) are basic; browser-side checks appear insufficient.
        *   *Examples:* Using small/clipped windows or manipulating input field display/cache. (VRP: `1395164`, `1358647`; VRP2.txt#1108181, #6717, #3801).

*   **Cross-Origin Data Leakage/Interaction (Medium Likelihood, High VRP):** Information from one origin's Autofill data being accessible to another origin, or actions in one frame affecting Autofill in another. Often involves iframes.
    *   **VRP Pattern (iframe Boundaries):** Bugs in how Autofill data is associated with specific frames (`RenderFrameHost`) or how suggestions are filtered based on origin.
        *   *Examples:* Incorrect `SubmittedOrigin` checks (VRP: `1301164`). Incorrect frame focus tracking leading to Autofill triggering on the wrong frame (VRP: `1474196`). Insufficient sender checks for IPC messages.
    *   **VRP Pattern (Password Manager Interaction):** Leaking password existence or username hints across origins via Autofill previews or interactions. (VRP: `1487110`). See [password_manager.md](password_manager.md).

*   **Incorrect Data Filling/Saving:** Autofill data being filled into the wrong fields, or data being saved under the wrong origin/profile. (Less common in recent VRPs).
*   **Bypassing Security Prompts:** Circumventing prompts required for sensitive actions (e.g., CVC unmasking, virtual card enrollment). (Focus area, but fewer recent public VRPs).

## 3. Further Analysis and Potential Issues
*   **Input Protection:** **Still the most critical area.** Investigate the current state of input protection (e.g., use of `InputEventActivationProtector`, minimum visibility duration checks like 500ms) on Autofill popups and prompts. Are they consistently applied across platforms (Desktop/Android)? Are they robust against all input methods (clicks, taps, keys, EyeDropper, etc.) and obscuring UI (PiP, FedCM, Permissions, Extensions)? (Related to VRP: `339481295`, VRP2.txt#7963, #825, #9878, etc.). **Regressions are common here (VRP: `40063230`).**
*   **Geometric Visibility Checks:** Where are robust visibility checks performed *now*? Renderer (`AutofillAgent`)? Browser Manager (`BrowserAutofillManager`)? UI Delegate (`AutofillExternalDelegate`)? UI Controller (`AutofillPopupControllerImpl`)? UI View (`AutofillPopupViewViews`)? How effective are they against clipping, off-screen positioning, zero opacity, same-origin overlays? Can checks be bypassed by manipulating render timing?
*   **Occlusion Detection:** How does Autofill detect if its UI is obscured by *other browser UI* (PiP, FedCM, Permissions)? Is the detection robust and timely? Does it correctly hide or disable interaction when obscured? Are there race conditions?
*   **iframe Origin/Focus Checks:** Re-examine cross-origin iframe scenarios. How is `FormStructure::host_frame()` token used? Are origin checks in `AutofillPopupControllerImpl::GetSuggestions` sufficient? How does focus tracking (`FocusedElementChanged`, `DidCompleteFocusChangeInFrame`) handle rapid changes or detached frames? Can IPC messages be spoofed by a compromised renderer? (Related to VRP: `1301164`, `1474196`).
*   **Preview State Security:** Are there *any* remaining side channels (timing, resource usage, CSS properties not explicitly reset) that could leak information from preview text? Re-evaluate `::first-line` and `@font-face` interactions.
*   **Android Keyboard Accessory/Bottom Sheet:** Does this UI have adequate input protection and visibility checks? (Related to VRP2.txt#13367).
*   **ListBox (`<select>`):** Does `scrollTop` still leak previewed selection? (Related to VRP: `1250850`).

## 4. Code Analysis
*   `components/autofill/content/renderer/autofill_agent.cc`: Renderer logic. `ShowSuggestions` checks basic field state. **Historically lacked geometric visibility checks.** Sends `AskForValuesToFill` IPC.
*   `components/autofill/content/browser/content_autofill_driver.cc`: Browser driver per RFH. `Lift` function adds frame tokens/origins.
*   `components/autofill/core/browser/foundations/browser_autofill_manager.cc`: Core browser logic. `OnAskForValuesToFillImpl`. `BuildSuggestionsContext`. `GetAvailable...Suggestions`. Calls `external_delegate_->OnSuggestionsReturned`. **Historically didn't perform robust visibility checks, relies heavily on UI layer.**
*   `components/autofill/core/browser/ui/autofill_external_delegate.h`: Interface to UI. Stores `caret_bounds`. `OnSuggestionsReturned`.
*   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc`: Manages popup UI state. Filters suggestions based on origin (`GetMatchingFrontendIDs`). Hides popup on certain events (`HidePopupWithoutAnimating`). **Crucial for origin checks and UI lifecycle management.**
*   `chrome/browser/ui/views/autofill/popup/autofill_popup_view_views.cc` (or similar): Desktop UI view. **Key location to check for `InputEventActivationProtector` usage and visibility/occlusion handling.**
*   `chrome/browser/autofill/android/` (`java/`): Android UI (PopupBridge, Keyboard Accessory). **Needs separate checks for input protection and visibility.**
*   `FormStructure`: Represents parsed form, holds origin info. Accuracy is vital.
*   `third_party/blink/renderer/core/input/event_handler.cc`: Handles mouse/touch events that might trigger Autofill.
*   `content/browser/renderer_host/input/input_router_impl.cc`: Routes input events.

## 5. Areas Requiring Further Investigation
*   **Input Protection Audit:** Systematically verify input protection mechanisms across all Autofill surfaces (desktop popup, Android popup, keyboard accessory, card unmask prompts, save prompts) against known bypass vectors (EyeDropper, taps, keyboard events, obscured UI). **Test for regressions.**
*   **Visibility Check Audit:** Identify current visibility check locations and methods. Test their effectiveness against various hiding techniques. Propose improvements if gaps exist.
*   **iframe Focus/Origin:** Create complex iframe tests (nested, cross-origin, sandboxed, dynamic manipulation, redirects) targeting `ContentAutofillDriver` data lifting and `AutofillPopupControllerImpl` origin filtering.
*   **Obscuring Interactions:** Systematically test Autofill UI obscured by PiP, FedCM (bubble/modal), Permissions (PEPC/address bar), Extension popups. Test keyboard interactions (`Enter`, `Tab`, Arrow keys) while obscured.
*   **Preview Side Channels:** Re-evaluate potential timing or rendering side channels related to preview text, especially involving CSS pseudo-elements or complex font features.
*   **Android UI:** Specifically audit the Keyboard Accessory and Bottom Sheet implementations for input protection and visibility flaws.

## 6. Related VRP Reports
*   **Interaction Bypass (EyeDropper):** VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825, #13922
*   **Interaction Bypass (Tap):** VRP: `40060134`, `40058217`, `40056900`; VRP2.txt#1426679, #1487440, #9878, #13367 (Keyboard Accessory)
*   **Interaction Bypass (Space Key):** VRP: `40056936`
*   **Interaction Bypass (Pointer Lock):** VRP: `40056870`
*   **Interaction Bypass (Custom Cursor):** VRP2.txt#13795
*   **Interaction Bypass (Timing - Move Input):** VRP: `40058217`, `40056900`; VRP2.txt#10877
*   **UI Obscuring (PiP):** VRP: `40058582`; VRP2.txt#5228
*   **UI Obscuring (FedCM):** VRP: `339481295`, `340893685`; VRP2.txt#7963
*   **Visibility Bypass (General):** VRP: `1395164`, `1358647`, `1108181`; VRP2.txt#6717, #3801
*   **Preview Leaks (Historical - scrollWidth/CSS):** VRP: `1035058`, `1035063`, `1013882`, `951487`, `916838`; VRP2.txt#4826, #6190, #6157
*   **Preview Leaks (Historical - select scrollTop):** VRP: `1250850`
*   **Preview Leaks (Side-channel):** VRP2.txt#14122, #14026
*   **Cross-Origin (iframe focus):** VRP: `1474196`
*   **Cross-Origin (SubmittedOrigin):** VRP: `1301164`
*   **Cross-Origin (Password Hints):** VRP: `1487110`
*   **Regression Example:** VRP: `40063230` (Regression of EyeDropper bypass fix)

## 7. Cross-References
*   [fedcm.md](fedcm.md)
*   [permissions.md](permissions.md)
*   [picture_in_picture.md](picture_in_picture.md)
*   [password_manager.md](password_manager.md)
*   [privacy.md](privacy.md)
*   [input.md](input.md)
*   [eye_dropper.md](eye_dropper.md)
*   [pointer_lock.md](pointer_lock.md)
