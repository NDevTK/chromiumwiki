# Component: Autofill Core Logic

## 1. Component Focus
*   **Functionality:** Core browser-side logic for parsing forms, identifying fillable fields, matching stored user data (addresses, credit cards, etc.), and managing autofill suggestions/previews. Excludes the direct UI rendering covered in [autofill_ui.md](autofill_ui.md).
*   **Key Logic:** Form parsing (`FormStructure`, `AutofillScanner`), field type classification (`AutofillField`), data matching (`AutofillProfileComparator`, `CreditCard`), suggestion generation (`AutofillPopupControllerImpl`), data validation (`AddressDataUtil`, `CreditCard`).
*   **Core Files:**
    *   `components/autofill/core/browser/` (esp. `form_structure.cc`, `autofill_field.cc`, `autofill_manager.cc`, `data_model/`)
    *   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc` (suggestion logic)

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing User Interaction Requirements (High VRP Frequency):** A primary goal of autofill security is ensuring intentional user action before filling sensitive data. Attackers frequently attempt to bypass these checks by manipulating input events or exploiting timing/state issues. This area is heavily targeted and prone to regressions.
    *   **VRP Pattern (Input Method Abuse):** Various APIs and input methods used to trigger autofill without expected user interaction (mouse movement, specific key presses).
        *   *EyeDropper API:* Used to bypass interaction requirements (VRP: `40065604`, `40063230`, `40058496`; VRP2.txt Line 825). See [eye_dropper.md](eye_dropper.md). Interaction logic in `AutofillPopupControllerImpl`.
        *   *Tap Events:* Double-taps or positioning the prompt under/near the cursor during taps bypassed checks (VRP: `40060134`, `40058217`, `40056900`; VRP2.txt Line 9878, 1426679). Focus on `AutofillPopupViewNativeViews::OnGestureEvent` and its interaction with timing protections. Android Keyboard Accessory/Bottom Sheet interaction (VRP2.txt Line 13367). Tapjacking permission element (VRP2.txt#7863).
        *   *Space Key:* Used to position prompt under cursor (VRP: `40056936`). Check `AutofillPopupControllerImpl::HandleKeyPressEvent`.
        *   *Pointer Lock:* Used to bypass mouse/keyboard input requirements (VRP: `40056870`). See [pointer_lock.md](pointer_lock.md).
    *   **VRP Pattern (Timing/State Issues):** Exploiting delays or specific states during the autofill process.
        *   *Prompt Positioning Delay:* Moving the input field *after* `mousedown` but *before* prompt position calculation allowed rendering the prompt under/near the cursor (VRP: `40058217`, `40056900`, VRP2.txt Line 10877). Analyze popup positioning logic timing relative to input events.
        *   *Interaction Delays:* Bypasses related to the 500ms delay intended to prevent accidental clicks (`kIgnoreEarlyClicksOnSuggestionsDuration`, `InputEventActivationProtector`). EyeDropper API was used to bypass this (VRP: `40063230`). Keyboard accessory interaction might bypass it (VRP2.txt Line 13367). Check enforcement in `AutofillPopupControllerImpl::AcceptSuggestion`.
*   **Leaking Autofill Preview Data (Medium VRP Frequency):** Exfiltrating suggested autofill data *before* the user explicitly selects it, often via side channels.
    *   **VRP Pattern (DOM/CSS Side-Channels):**
        *   *`scrollWidth` Leak:* Preview text affecting `scrollWidth` even when `.value` is empty (VRP: `916838`). Subsequent fixes attempted to mock `scrollWidth`, but scrollbar presence still leaked info (VRP2.txt#4826).
        *   *Font Manipulation:* Bypassing font pinning (`system-ui`) by overriding `@font-face` for `system-ui` (VRP2.txt#6190) or using `::first-line` (VRP2.txt#6157) combined with `scrollWidth` measurements. Requires analysis of how preview text styling is applied and isolated.
        *   *`<select>` `scrollTop` Leak:* Previewed `<select>` option revealing selected index via `scrollTop` changes (VRP: `1250850`). Check `SelectFillFunction`).
    *   **VRP Pattern (Other Timing/Side-Channels):** Potential for other subtle leaks during preview state (VRP2.txt Line 14122).
*   **Form Parsing & Field Classification Errors:** Incorrect parsing of forms (`FormStructure`, `AutofillScanner`) or misclassification of field types (`AutofillField::ParseFieldTypesFromAutocompleteAttribute`, heuristics) could lead to incorrect data being suggested, filled, or potentially leaked.
*   **Data Validation Failures:** Insufficient validation of stored data (addresses, credit cards) or user input before filling could lead to unexpected behavior or injection vulnerabilities if data is rendered insecurely later.
    *   Address Validation: Robustness of state name mapping (`AlternativeStateNameMap`), zip code regex (`IsValidZip`), address rewriting (`AddressRewriter`).
*   **Race Conditions/State Management:** Complex state interactions (e.g., during suggestion fetching, display, acceptance, hiding) might have race conditions leading to incorrect behavior or security bypasses. Focus on `AutofillPopupControllerImpl` state machine.

## 3. Further Analysis and Potential Issues
*   **Interaction Requirement Robustness:** Are the checks preventing autofill without intentional user interaction (mouse move over item, key press selection) robust against all possible input event sequences, API interactions (EyeDropper, PointerLock), and timing manipulations? How is the `InputEventActivationProtector` applied and can it be bypassed?
*   **Preview Data Isolation:** How completely is preview text isolated from the web page? Audit all potential side-channels (DOM properties like `scrollWidth`, `offsetHeight`, CSS interactions like font loading, layout timing) that might leak information about preview content dimensions or characters.
*   **Form Parsing Security:** How resilient is `FormStructure` and field classification logic to malformed HTML or tricky autocomplete attributes designed to confuse the parser?
*   **Data Validation:** Are all components of stored addresses/cards validated before use? Is validation locale-aware where necessary?
*   **Concurrency:** Analyze the handling of concurrent autofill events or interactions (e.g., multiple fields showing suggestions, interactions with other UI).

## 4. Code Analysis
*   `AutofillPopupControllerImpl`: Central class managing popup state, suggestions, user interaction. Key methods: `GetSuggestions`, `AcceptSuggestion`, `HandleKeyPressEvent`. Contains logic for interaction delays (`kIgnoreEarlyClicksOnSuggestionsDuration`) and potentially visibility checks.
*   `AutofillPopupViewNativeViews` (or platform equivalent): Handles UI events like `OnMouseMoved`, `OnGestureEvent`. Interaction with `AutofillPopupControllerImpl` is critical.
*   `FormStructure`, `AutofillScanner`: Responsible for parsing HTML forms to identify fillable fields.
*   `AutofillField`: Represents a field, determines its type (`ParseFieldTypesFromAutocompleteAttribute`), and holds heuristics.
*   `AddressDataUtil`, `CreditCard`, `AutofillProfileComparator`: Core data models and validation logic. `AlternativeStateNameMap`, `AddressRewriter`.
*   `SelectFillFunction`: Logic for filling `<select>` elements, relevant to `scrollTop` leak (VRP: `1250850`).

## 5. Areas Requiring Further Investigation
*   Audit `AutofillPopupControllerImpl::AcceptSuggestion` and related event handlers (`HandleKeyPressEvent`, `OnMouseMoved`, `OnGestureEvent` in Views) for robust enforcement of interaction requirements across all input types (mouse, keyboard, tap, stylus, accessibility tools).
*   Systematically investigate all DOM/CSS properties that could potentially leak information about preview text (dimensions, content changes) when applied to input/textarea elements in the preview state.
*   Review `FormStructure` parsing logic for robustness against adversarial HTML.
*   Test data validation routines (`AddressDataUtil`, `CreditCard` validation) with edge cases and international data.
*   Fuzzing autofill interaction flows, focusing on timing and concurrent events.

## 6. Related VRP Reports (Selected Patterns)
*   **Interaction Bypass (EyeDropper):** VRP: `40065604`, `40063230`, `40058496`; VRP2.txt Line 825
*   **Interaction Bypass (Tap/Click):** VRP: `40060134`, `40058217`, `40056900`; VRP2.txt Line 9878, 10877, 1426679, 1487440
*   **Interaction Bypass (Pointer Lock):** VRP: `40056870`
*   **Interaction Bypass (Space Key):** VRP: `40056936`
*   **Interaction Bypass (Keyboard Accessory):** VRP2.txt#13367
*   **Preview Leak (scrollWidth/Font):** VRP: `1035058`, `1035063`, `1013882`, `951487`, `916838`; VRP2.txt Line 4826, 6190, 6157
*   **Preview Leak (Select scrollTop):** VRP: `1250850`, VRP2.txt#7191
*   **Preview Leak (General):** VRP2.txt#14122, #14026

*(See also linked pages like [autofill_ui.md](autofill_ui.md) for UI obscuring issues)*
