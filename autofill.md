# Component: Autofill (Core Logic & UI Interaction)

## 1. Component Focus
*   **Functionality:** Handles form filling for addresses, credit cards, passwords (via Password Manager interaction), etc. Includes parsing forms, identifying fillable fields, matching stored user data, managing suggestions/previews, rendering UI elements (popups, dialogs, snackbars), handling user interaction for selection/filling, and implementing security measures against leaks and bypasses.
*   **Key Logic:** Form parsing (`FormStructure`, `AutofillScanner`), field type classification (`AutofillField`), data matching (`AutofillProfileComparator`, `CreditCard`), suggestion generation and UI control (`AutofillPopupControllerImpl`), UI rendering and event handling (`AutofillPopupView` implementations), data validation (`AddressDataUtil`, `CreditCard`), interaction requirement enforcement (including `kIgnoreEarlyClicksOnSuggestionsDuration` delay).
*   **Core Files:**
    *   `components/autofill/core/browser/` (esp. `form_structure.cc`, `autofill_field.cc`, `autofill_manager.cc`, `data_model/`)
    *   `chrome/browser/ui/autofill/` (`autofill_popup_controller_impl.cc`, `autofill_suggestion_controller.h`, `autofill_popup_view.h`, `autofill_keyboard_accessory_controller_impl.cc`, `autofill_snackbar_controller_impl.cc`, `popup_view_views.cc`, etc.)
    *   `components/autofill/core/browser/ui/payments/` (dialog controllers)
    *   `chrome/browser/ui/views/payments/payment_request_sheet_controller.cc` (Related UI)

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing User Interaction Requirements (High VRP Frequency):** A primary goal is ensuring intentional user action before filling sensitive data. Attackers frequently target checks requiring user gestures (mouse movement, clicks, taps, key presses). A key defense is the `kIgnoreEarlyClicksOnSuggestionsDuration` (500ms) delay meant to prevent accidental clicks/taps.
    *   **VRP Pattern (Input Method Abuse):** Various APIs/inputs used to trigger autofill without expected gestures, often bypassing the interaction delay.
        *   *EyeDropper API:* Repeatedly used to bypass interaction checks, including the 500ms delay (VRP: `40065604`, `40063230` regression of `1287364`, `40058496`; VRP2.txt#825 was `1287364`). See [eye_dropper.md](eye_dropper.md).
        *   *Tap Events:* Double-taps or positioning prompt under cursor during taps bypassed checks (VRP: `40060134` bypass of `1240472`/`1279268`, `40058217`, `40056900`; VRP2.txt#9878, #1426679, #1487440). Android Keyboard Accessory interaction (VRP2.txt#13367).
        *   *Space Key:* Used with positioning (VRP: `40056936`).
        *   *Pointer Lock:* Used to bypass mouse/keyboard requirements (VRP: `40056870`). See [pointer_lock.md](pointer_lock.md).
        *   *Custom Cursors:* Obscuring prompt might facilitate interaction bypass (Related: VRP2.txt#13795).
    *   **VRP Pattern (Timing/State Issues):** Exploiting delays or specific states.
        *   *Prompt Positioning Delay:* Moving input field *after* `mousedown` but *before* prompt position calculation (VRP: `40058217` near cursor, `40056900` under cursor; VRP2.txt#10877).
        *   *Interaction Delays:* Specific bypasses of the `kIgnoreEarlyClicksOnSuggestionsDuration` protection (e.g., VRP: `40063230` via EyeDropper).
    *   **VRP Pattern (Regressions):** Autofill bypass vulnerabilities frequently reappear after fixes, often through slightly different interaction methods (e.g., VRP: `40063230` regression of `1287364`). This highlights the complexity of enforcing interaction requirements across all scenarios.
*   **UI Obscuring/Visibility Bypass (High VRP Frequency):** Attackers hiding the autofill prompt while still allowing interaction.
    *   **VRP Pattern (Overlaying UI Elements):** Other browser UI obscures the autofill prompt.
        *   *Picture-in-Picture (PiP):* (VRP: `40058582`, VRP2.txt#5228). Requires robust `OverlapsWithPictureInPictureWindow` checks in `AutofillPopupControllerImpl`. See [picture_in_picture.md](picture_in_picture.md).
        *   *FedCM:* (VRP: `339481295`, `340893685`, VRP2.txt#7963). See [fedcm.md](fedcm.md).
        *   *Extensions:* Inactive extension popups/windows obscure prompt (VRP2.txt#9002, #9101, #12352). See [extensions_api.md](extensions_api.md).
    *   **VRP Pattern (UI Manipulation/Clipping):** Manipulating page/browser state to hide the prompt.
        *   *Input Field Cache/Display:* Tricking visibility checks in `AutofillPopupControllerImpl` (VRP: `1358647`, VRP2.txt#6717, #3801).
        *   *Small Windows/Clipping (Android):* Docking/clipping prompt making it unreadable (VRP: `1395164`, VRP2.txt#10165). Requires view implementation to hide if clipped.
*   **Leaking Autofill Preview Data (Medium VRP Frequency):** Exfiltrating suggested data *before* explicit selection, often via side channels.
    *   **VRP Pattern (DOM/CSS Side-Channels):**
        *   *`scrollWidth` Leak:* (VRP: `916838`, VRP2.txt#7396).
        *   *Font Manipulation:* Bypassing font pinning via `@font-face` or `::first-line` combined with `scrollWidth` (VRP: `1013882`, `1035058`, `1035063`; VRP2.txt#6190, #6157).
        *   *`<select>` `scrollTop` Leak:* (VRP: `1250850`, VRP2.txt#7191). Check `SelectFillFunction`).
    *   **VRP Pattern (Other Timing/Side-Channels):** (VRP2.txt#14122, #14026).
*   **Form Parsing & Field Classification Errors:** Incorrect parsing (`FormStructure`) or misclassification (`AutofillField`) leading to wrong suggestions or leaks.
*   **Data Validation Failures:** Insufficient validation of stored data or user input before filling.
*   **Race Conditions/State Management:** Complex state interactions (suggestion fetching, display, acceptance, hiding) in `AutofillPopupControllerImpl` might have races.
*   **Data Leakage (Accessibility):** Sensitive data potentially exposed via accessibility events (`FireControlsChangedEvent`).

## 3. Further Analysis and Potential Issues
*   **Interaction Requirement Robustness:** Are checks preventing autofill without intentional interaction robust across all input methods (clicks, taps, keys, EyeDropper, PointerLock, etc.) and timing manipulations? How consistently is the `kIgnoreEarlyClicksOnSuggestionsDuration` delay enforced by `NextIdleBarrier` in `AutofillPopupControllerImpl`, `AutofillKeyboardAccessoryControllerImpl` and `PopupRowView`? (VRP bypasses: `40063230`, `40060134`, `40056870`).
*   **Popup Visibility and Occlusion:** How robust are occlusion checks (`OverlapsWithPictureInPictureWindow`)? How is triggering element visibility determined by the controller (`AutofillPopupControllerImpl`)? How is clipping handled on Android? (VRP bypasses: `40058582`, `339481295`, `1395164`)
*   **Preview Data Isolation:** Audit potential side-channels (DOM properties, CSS, layout timing) for leaking preview content. (VRP leaks: `916838`, `1013882`, `1250850`).
*   **Form Parsing Security:** Resilience of `FormStructure` and field classification to adversarial HTML/attributes.
*   **Data Validation:** Robustness of address/card validation routines.
*   **Concurrency:** Handling concurrent autofill events.
*   **Snackbar/Dialog Security:** Review `AutofillSnackbarControllerImpl`, `AutofillErrorDialogControllerImpl`, etc. for data display sanitization, state management races, and secure callback handling.
*   **Sub-popup Security:** Lifecycle management and isolation for sub-popups.
*   **Regressions:** Given the history, specifically test edge cases and interaction variations after fixes related to interaction requirements or UI obscuring.

## 4. Code Analysis
*   `AutofillPopupControllerImpl`: Manages popup state, suggestions, interaction delays (`kIgnoreEarlyClicksOnSuggestionsDuration` via `barrier_for_accepting_`), visibility checks (`OverlapsWithPictureInPictureWindow`?). Key methods: `Show`, `Hide`, `GetSuggestions`, `AcceptSuggestion`, `HandleKeyPressEvent`.
*   `AutofillSuggestionController`: Defines constants like `kIgnoreEarlyClicksOnSuggestionsDuration`.
*   `AutofillPopupViewNativeViews` (or platform equivalent): Handles UI events (`OnMouseMoved`, `OnGestureEvent`, `HandleKeyPressEventForCompose`), renders suggestions (`OnSuggestionsChanged`), checks clipping/occlusion.
*   `AutofillKeyboardAccessoryControllerImpl`: Handles keyboard accessory, also uses `kIgnoreEarlyClicksOnSuggestionsDuration`.
*   `PopupRowView`: Individual row view, also uses `kIgnoreEarlyClicksOnSuggestionsDuration`.
*   `FormStructure`, `AutofillScanner`: HTML form parsing.
*   `AutofillField`: Field classification (`ParseFieldTypesFromAutocompleteAttribute`).
*   `AddressDataUtil`, `CreditCard`, `AutofillProfileComparator`: Data models and validation.
*   `SelectFillFunction`: Fills `<select>` elements (VRP: `1250850`).
*   `AutofillSnackbarControllerImpl`, `AutofillErrorDialogControllerImpl`, `AutofillProgressDialogControllerImpl`: Other UI components.

## 5. Areas Requiring Further Investigation
*   Audit `AutofillPopupControllerImpl::AcceptSuggestion`, `AutofillKeyboardAccessoryControllerImpl`, `PopupRowView` event handlers for robust interaction requirement enforcement, especially concerning the `kIgnoreEarlyClicksOnSuggestionsDuration` delay across all input methods.
*   Systematically investigate DOM/CSS side channels for preview data leaks.
*   Review `FormStructure` parsing logic for robustness.
*   Test data validation routines with edge cases.
*   Fuzz autofill interaction flows.
*   Verify robustness of occlusion/visibility checks in `AutofillPopupControllerImpl` and `AutofillPopupView`.
*   Audit security of snackbars and dialogs.
*   Confirm accessibility events (`FireControlsChangedEvent`) don't leak sensitive data.

## 6. Related VRP Reports (Selected Patterns)
*   **Interaction Bypass (EyeDropper):** VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825 (`1287364`)
*   **Interaction Bypass (Tap/Click/Positioning):** VRP: `40060134`, `40058217`, `40056900`; VRP2.txt#9878, #10877, #1426679, #1487440
*   **Interaction Bypass (Pointer Lock):** VRP: `40056870`
*   **Interaction Bypass (Space Key):** VRP: `40056936`
*   **Interaction Bypass (Keyboard Accessory):** VRP2.txt#13367
*   **UI Obscuring (PiP):** VRP: `40058582`, VRP2.txt#5228
*   **UI Obscuring (FedCM):** VRP: `339481295`, `340893685`, VRP2.txt#7963
*   **UI Obscuring (Extension Window):** VRP2.txt#9002, #9101, #12352
*   **Visibility Bypass (Clipping/Cache):** VRP: `1395164`, `1358647`; VRP2.txt#1108181, #6717, #3801, #10165 (Android)
*   **Preview Leak (scrollWidth/Font):** VRP: `1035058`, `1035063`, `1013882`, `951487`, `916838`; VRP2.txt#4826, #6190, #6157
*   **Preview Leak (Select scrollTop):** VRP: `1250850`, VRP2.txt#7191
*   **Preview Leak (General):** VRP2.txt#14122, #14026

*(Note: This merged page now covers both core logic and UI interaction vulnerabilities for Autofill.)*
