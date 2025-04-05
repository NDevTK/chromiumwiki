# Autofill Security Analysis

**Component Focus:** Chromium autofill, including core logic (`components/autofill/core/browser/`), data handling, and UI (`chrome/browser/ui/autofill/`, `chrome/browser/ui/views/payments/`). Key files include `autofill_popup_controller_impl.cc`/`.h` (popup), `payment_request_sheet_controller.cc` (payment sheet UI), and `form_structure.cc`/`.h` (form parsing). High VRP vulnerability count, particularly around UI interactions and bypasses.

**Related Wiki Pages:** [autofill_ui.md](autofill_ui.md), [payments.md](payments.md), [eye_dropper.md](eye_dropper.md), [pointer_lock.md](pointer_lock.md), [picture_in_picture.md](picture_in_picture.md), [fedcm.md](fedcm.md), [extension_security.md](extension_security.md)

## Potential Security Issues & VRP Patterns:

*   **Bypassing User Interaction Requirements (High VRP Frequency):** A primary goal of autofill security is ensuring intentional user action before filling sensitive data. Attackers frequently attempt to bypass these checks.
    *   **VRP Pattern (Input Method Abuse):** Various APIs and input methods have been used to trigger autofill without expected user interaction (mouse movement, specific key presses).
        *   **EyeDropper API:** Used to bypass interaction requirements (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt Line 825). See [eye_dropper.md](eye_dropper.md).
        *   **Tap Events:** Double-taps or positioning the prompt under the cursor during taps bypassed checks (VRP: `40060134`, `40058217`, `40056900`, `VRP2.txt#1426679`, `VRP2.txt#1487440`, `VRP2.txt#9878`). Android Keyboard Accessory/Bottom Sheet interaction (VRP2.txt Line 13367).
        *   **Space Key:** Used to position prompt under cursor (VRP: `40056936`).
        *   **Pointer Lock:** Used to bypass mouse/keyboard input requirements (VRP: `40056870`). See [pointer_lock.md](pointer_lock.md).
    *   **VRP Pattern (Timing/State Issues):** Exploiting delays or specific states during the autofill process.
        *   **Prompt Positioning Delay:** Moving the input field *after* `mousedown` but *before* prompt position calculation allowed rendering the prompt under/near the cursor (VRP: `40058217`, `40056900`, VRP2.txt Line 10877).
        *   **`NextIdleBarrier`:** While intended to prevent accidental clicks immediately after popup display (`AcceptSuggestion`), its effectiveness under concurrent events needs scrutiny (See Race Conditions below).
        *   **Keyboard Input Handling (`HandleKeyPressEvent`):** Needs thorough auditing for secure handling of all key events and combinations, especially those used in bypasses (like spacebar, VRP: `40056936`).
        *   **Specific Research Question:** How can `HandleKeyPressEvent` be hardened against unexpected key combinations or sequences used in interaction bypasses identified in VRPs?

*   **Obscuring Autofill UI (High VRP Frequency):** Attackers attempt to hide the autofill prompt while still allowing interaction, preventing user awareness.
    *   **VRP Pattern (Overlaying UI Elements):** Other browser UI elements obscure the autofill prompt.
        *   **Picture-in-Picture (PiP):** Video/Document PiP windows shown over the prompt (VRP: `40058582`). See [picture_in_picture.md](picture_in_picture.md).
        *   **FedCM:** FedCM bubble dialog shown over the prompt (VRP: `339481295`, `340893685`, `VRP2.txt#7963`). See [fedcm.md](fedcm.md).
        *   **Extension Windows:** Inactive extension popups (VRP2.txt Line 9002, ref `1290213`) or off-screen windows (VRP2.txt Line 9101) obscure the prompt while allowing keyboard interaction. See [extension_security.md](extension_security.md).
    *   **VRP Pattern (UI Manipulation/Clipping):** Manipulating the page or browser state to hide the prompt.
        *   **Input Field Cache/Display:** Tricking visibility checks (VRP: `1395164`, `1358647`, `VRP2.txt#1108181`, `VRP2.txt#6717`, VRP2.txt#3801).
        *   **Small Windows/Clipping (Android):** Docking/clipping the prompt in narrow views (VRP: `1395164`).
    *   **Occlusion Checks:** Investigation needed into the robustness of checks designed to prevent filling when obscured (e.g., interaction with PiP occlusion tracking).
    *   **Specific Research Question:** Are the occlusion checks robust against all methods of overlaying (PiP, FedCM, Extension windows, other dialogs)? How can these checks be improved?

*   **Leaking Autofill Preview Data (Medium VRP Frequency):** Exfiltrating suggested autofill data *before* the user explicitly selects it.
    *   **VRP Pattern (Timing/Side-Channels):**
        *   **`scrollWidth` Leak:** Preview text affecting `scrollWidth` even when `.value` is empty (VRP: `916838`).
        *   **Font Manipulation:** Bypassing font pinning (`system-ui`) by overriding `@font-face` (VRP2.txt Line 6190) or using `::first-line` (VRP2.txt Line 6157) combined with `scrollWidth` measurements.
        *   **`<select>` `scrollTop` Leak:** Previewed `<select>` option revealing index via `scrollTop` (VRP: `1250850`).
        *   **Side-Channel via Preview:** General potential for timing or other side-channels (VRP2.txt Line 14122).
    *   **Specific Research Question:** Can other CSS properties or DOM element properties leak information about preview text dimensions or content?

*   **Input Validation Failures:** Ensuring data used by autofill (both user input and fetched data) is valid and sanitized.
    *   **Filter Strings (`FilterSuggestions`):** Uses `base::i18n::ToLower`. Need to verify robustness against injection across locales.
        *   **Specific Research Question:** Is `base::i18n::ToLower` sufficient sanitization against sophisticated injection attempts in `FilterSuggestions`, considering diverse locales?
    *   **Address Validation:** Focus on robustness of state name (`AlternativeStateNameMap`), US zip code (`IsValidZip` regex), and address rewriting (`AddressRewriter`) logic. Check for gaps in validation for other fields (street, city, country).
        *   **Specific Research Question:** How securely are the `AlternativeStateNameMap` protobuf files loaded and updated? Can the `IsValidZip` regex be bypassed? Can `AddressRewriter` normalization introduce vulnerabilities?
    *   **Autocomplete Attribute:** Ensure `SetFieldTypesFromAutocompleteAttribute` handles potentially malicious `autocomplete` attribute values securely.

*   **Cross-Site Scripting (XSS):** Ensuring data displayed in autofill UI doesn't introduce script execution.
    *   **Popup & Payment Sheet Display:** Data displayed in popups (`AcceptSuggestion`) or payment sheets (`payment_request_sheet_controller.cc`) needs sanitization. `suggestion.acceptance_a11y_announcement` seems low risk as it uses localized strings, but other dynamic content display needs review.
        *   **Specific Research Question:** Review all UI display functions in `payment_request_sheet_controller.cc` for proper sanitization of dynamic data.

*   **Race Conditions:** Potential for UI inconsistencies or security bypasses due to timing issues.
    *   **Popup Visibility (`Show()`/`Hide()`):** Concurrent calls could lead to issues. Mitigations (`AutofillPopupHideHelper`, `NextIdleBarrier`, weak pointers, async deletion) need validation under heavy load/concurrency. The `kIgnoreEarlyClicksOnSuggestionsDuration` aims to prevent accidental acceptance.
        *   **Specific Research Question:** Can the `NextIdleBarrier` or `kIgnoreEarlyClicksOnSuggestionsDuration` be bypassed under specific timing conditions or event sequences?

*   **Regressions & Bypass Patterns:** Autofill is prone to regressions where fixes for one bypass method are circumvented by slightly different techniques (e.g., multiple EyeDropper bypasses VRP: `40065604`, `40063230`; tap bypasses VRP: `40060134`). Rigorous testing after fixes is crucial.

## Code Analysis

*(Existing code analysis sections for `FilterSuggestions`, `AcceptSuggestion`, `FormStructure` remain relevant but should be viewed through the lens of the VRP patterns above)*

### Key Files:

*   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc`/`.h`: Popup logic, event handling (`HandleKeyPressEvent`, `AcceptSuggestion`), visibility (`Show`, `Hide`), filtering (`FilterSuggestions`).
*   `components/autofill/core/browser/form_structure.cc`/`.h`: Form parsing, field type determination, signature calculation.
*   `components/autofill/core/browser/data_model/`: Address, Credit Card validation logic (`alternative_state_name_map.cc`, `address_rewriter.cc`, validation utils).
*   `chrome/browser/ui/views/payments/payment_request_sheet_controller.cc`: Payment sheet UI.
*   `content/public/browser/render_widget_host_view.h`: Interface for view interactions, potentially relevant to visibility/occlusion.

**Vulnerability Note:** High VRP payout history and frequent regressions highlight the critical nature and complexity of Autofill security.

## Related VRP Reports (Selected Patterns)

*   **Input Bypass (EyeDropper):** VRP: `40065604`, `40063230`, `40058496`; VRP2.txt Line 825
*   **Input Bypass (Tap):** VRP: `40060134`, `40058217`, `40056900`; VRP2.txt Line 9878, 1426679
*   **Input Bypass (Pointer Lock):** VRP: `40056870`
*   **UI Obscuring (PiP):** VRP: `40058582`
*   **UI Obscuring (FedCM):** VRP: `339481295`, `VRP2.txt#7963`
*   **UI Obscuring (Extension Window):** VRP2.txt Line 9002, 9101
*   **Preview Leak (scrollWidth/Font):** VRP2.txt Line 4826, 6190
*   **Preview Leak (Select scrollTop):** VRP: `1250850`
*   **Input Cache Bypass:** VRP: `1395164`, `1358647`; VRP2.txt Line 1108181, 6717, 3801

*(This list is illustrative, not exhaustive. Refer to `VRP.txt` and `VRP2.txt` for full details)*
