# Component: Autofill (Core Logic & UI Interaction)

## 1. Component Focus
*   **Functionality:** Handles form filling for addresses, credit cards, passwords (via Password Manager interaction), etc. Includes parsing forms, identifying fillable fields, matching stored user data, managing suggestions/previews, rendering UI elements (popups, dialogs, snackbars), handling user interaction for selection/filling, and implementing security measures against leaks and bypasses.
*   **Key Logic:** Form parsing (`FormStructure`, `AutofillScanner`), field type classification (`AutofillField`), data matching (`AutofillProfileComparator`, `CreditCard`), suggestion generation and UI control (`AutofillPopupControllerImpl`), UI rendering and event handling (`AutofillPopupView` implementations), data validation (`AddressDataUtil`, `CreditCard`), interaction requirement enforcement.
*   **Core Files:**
    *   `components/autofill/core/browser/` (esp. `form_structure.cc`, `autofill_field.cc`, `autofill_manager.cc`, `data_model/`)
    *   `chrome/browser/ui/autofill/` (`autofill_popup_controller_impl.cc`, `autofill_popup_view.h`, `autofill_snackbar_controller_impl.cc`, `popup_view_views.cc`, etc.)
    *   `components/autofill/core/browser/ui/payments/` (dialog controllers)
    *   `chrome/browser/ui/views/payments/payment_request_sheet_controller.cc` (Related UI)

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing User Interaction Requirements (High VRP Frequency):** A primary goal is ensuring intentional user action before filling sensitive data. Attackers frequently target checks requiring user gestures (mouse movement, clicks, taps, key presses).
    *   **VRP Pattern (Input Method Abuse):** Various APIs/inputs used to trigger autofill without expected gestures.
        *   *EyeDropper API:* Used to bypass interaction checks (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825). See [eye_dropper.md](eye_dropper.md).
        *   *Tap Events:* Double-taps or positioning prompt under cursor during taps bypassed checks (VRP: `40060134`, `40058217`, `40056900`; VRP2.txt#9878, #1426679, #1487440). Android Keyboard Accessory interaction (VRP2.txt#13367).
        *   *Space Key:* Used with positioning (VRP: `40056936`).
        *   *Pointer Lock:* Used to bypass mouse/keyboard requirements (VRP: `40056870`). See [pointer_lock.md](pointer_lock.md).
        *   *Custom Cursors:* Potentially interfering with interaction checks (VRP2.txt#13795).
    *   **VRP Pattern (Timing/State Issues):** Exploiting delays or specific states.
        *   *Prompt Positioning Delay:* Moving input field *after* `mousedown` but *before* prompt position calculation (VRP: `40058217`, `40056900`, VRP2.txt#10877).
        *   *Interaction Delays:* Bypasses related to the 500ms delay (`kIgnoreEarlyClicksOnSuggestionsDuration`, `InputEventActivationProtector`). EyeDropper was a key bypass (VRP: `40063230`).
*   **UI Obscuring/Visibility Bypass (High VRP Frequency):** Attackers hiding the autofill prompt while still allowing interaction.
    *   **VRP Pattern (Overlaying UI Elements):** Other browser UI obscures the autofill prompt.
        *   *Picture-in-Picture (PiP):* (VRP: `40058582`, VRP2.txt#5228). Requires robust `OverlapsWithPictureInPictureWindow` checks in `AutofillPopupControllerImpl`. See [picture_in_picture.md](picture_in_picture.md).
        *   *FedCM:* (VRP: `339481295`, `340893685`, `VRP2.txt#7963`). See [fedcm.md](fedcm.md).
        *   *Extensions:* Inactive extension popups/windows obscure prompt (VRP2.txt#9002, #9101, #12352). See [extensions_api.md](extensions_api.md).
    *   **VRP Pattern (UI Manipulation/Clipping):** Manipulating page/browser state to hide the prompt.
        *   *Input Field Cache/Display:* Tricking visibility checks in `AutofillPopupControllerImpl` (VRP: `1395164`, `1358647`, VRP2.txt#1108181, #6717, #3801).
        *   *Small Windows/Clipping (Android):* Docking/clipping prompt making it unreadable (VRP: `1395164`, VRP2.txt#10165). Requires view implementation to hide if clipped.
*   **Leaking Autofill Preview Data (Medium VRP Frequency):** Exfiltrating suggested data *before* explicit selection, often via side channels.
    *   **VRP Pattern (DOM/CSS Side-Channels):**
        *   *`scrollWidth` Leak:* (VRP: `916838`, VRP2.txt#4826).
        *   *Font Manipulation:* Bypassing font pinning via `@font-face` or `::first-line` combined with `scrollWidth` (VRP2.txt#6190, #6157).
        *   *`<select>` `scrollTop` Leak:* (VRP: `1250850`). Check `SelectFillFunction`).
    *   **VRP Pattern (Other Timing/Side-Channels):** (VRP2.txt#14122, #14026).
*   **Form Parsing & Field Classification Errors:** Incorrect parsing (`FormStructure`) or misclassification (`AutofillField`) leading to wrong suggestions or leaks.
*   **Data Validation Failures:** Insufficient validation of stored data or user input before filling.
*   **Race Conditions/State Management:** Complex state interactions (suggestion fetching, display, acceptance, hiding) in `AutofillPopupControllerImpl` might have races.
*   **Data Leakage (Accessibility):** Sensitive data potentially exposed via accessibility events (`FireControlsChangedEvent`).

## 3. Further Analysis and Potential Issues
*   **Interaction Requirement Robustness:** Are checks preventing autofill without intentional interaction robust across all input methods and timing manipulations? How is `InputEventActivationProtector` / `kIgnoreEarlyClicksOnSuggestionsDuration` applied?
*   **Popup Visibility and Occlusion:** How robust are occlusion checks (`OverlapsWithPictureInPictureWindow`)? How is triggering element visibility determined by the controller (`AutofillPopupControllerImpl`)? How is clipping handled on Android?
*   **Preview Data Isolation:** Audit potential side-channels (DOM properties, CSS, layout timing) for leaking preview content.
*   **Form Parsing Security:** Resilience of `FormStructure` and field classification to adversarial HTML/attributes.
*   **Data Validation:** Robustness of address/card validation routines.
*   **Concurrency:** Handling concurrent autofill events.
*   **Snackbar/Dialog Security:** Review `AutofillSnackbarControllerImpl`, `AutofillErrorDialogControllerImpl`, etc. for data display sanitization, state management races, and secure callback handling.
*   **Sub-popup Security:** Lifecycle management and isolation for sub-popups.

## 4. Code Analysis
*   `AutofillPopupControllerImpl`: Manages popup state, suggestions, interaction delays (`kIgnoreEarlyClicksOnSuggestionsDuration`?), visibility checks (`OverlapsWithPictureInPictureWindow`?). Key methods: `Show`, `Hide`, `GetSuggestions`, `AcceptSuggestion`, `HandleKeyPressEvent`.
*   `AutofillPopupViewNativeViews` (or platform equivalent): Handles UI events (`OnMouseMoved`, `OnGestureEvent`, `HandleKeyPressEventForCompose`), renders suggestions (`OnSuggestionsChanged`), checks clipping/occlusion.
*   `FormStructure`, `AutofillScanner`: HTML form parsing.
*   `AutofillField`: Field classification (`ParseFieldTypesFromAutocompleteAttribute`).
*   `AddressDataUtil`, `CreditCard`, `AutofillProfileComparator`: Data models and validation.
*   `SelectFillFunction`: Fills `<select>` elements (VRP: `1250850`).
*   `AutofillSnackbarControllerImpl`, `AutofillErrorDialogControllerImpl`, `AutofillProgressDialogControllerImpl`: Other UI components.

## 5. Areas Requiring Further Investigation
*   Audit `AutofillPopupControllerImpl::AcceptSuggestion` and related event handlers for robust interaction requirement enforcement across all input methods.
*   Systematically investigate DOM/CSS side channels for preview data leaks.
*   Review `FormStructure` parsing logic for robustness.
*   Test data validation routines with edge cases.
*   Fuzz autofill interaction flows.
*   Verify robustness of occlusion/visibility checks in `AutofillPopupControllerImpl` and `AutofillPopupView`.
*   Audit security of snackbars and dialogs.
*   Confirm accessibility events (`FireControlsChangedEvent`) don't leak sensitive data.

## 6. Related VRP Reports (Selected Patterns)
*   **Interaction Bypass (EyeDropper):** VRP: `40065604`, `40063230`, `40058496`; VRP2.txt#825
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
