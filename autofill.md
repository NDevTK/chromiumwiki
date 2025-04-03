# Autofill Security Analysis

**Component Focus:** Chromium autofill, including core logic, data handling, and UI. Key files are `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc` and `autofill_popup_controller_impl.h` (popup), and `chrome/browser/ui/views/payments/payment_request_sheet_controller.cc` (payment sheet UI). High VRP vulnerability count.

## Potential Security Issues:

*   **Input Validation Failures:**
    *   **`HandleKeyPressEvent`, `AcceptSuggestion`:** Input validation in `HandleKeyPressEvent` and `AcceptSuggestion` within `autofill_popup_controller_impl.cc` needs scrutiny to prevent injection attacks. Ensure all user inputs and suggestion data are properly validated and sanitized before being processed or displayed to prevent vulnerabilities.
        *   **Specific Research Questions:**
            * How thoroughly are user inputs validated in `HandleKeyPressEvent` to prevent injection attacks?
            * Are there any edge cases or bypasses in the input validation logic within `AcceptSuggestion`?
            * Investigate potential vulnerabilities related to handling non-ASCII characters or special characters in user inputs.
    *   **Filter Strings in `FilterSuggestions`:** The `FilterSuggestions` function in `autofill_popup_controller_impl.cc` uses `base::i18n::ToLower` for case-insensitive filtering, which offers some sanitization. However, further analysis is recommended to confirm its complete effectiveness against injection attacks, especially concerning diverse locale handling and potential future regex-based filtering implementations.
        *   **Specific Research Questions:**
            * Is `base::i18n::ToLower` sufficient for sanitizing filter strings against all potential injection vectors, especially with different locales?
            * Are there any scenarios where the filtering logic in `FilterSuggestions` could be bypassed to inject malicious strings?
            * If regex-based filtering is implemented in the future, how can it be secured against regex injection vulnerabilities?
    *   **Exempt Trigger Sources:** Certain trigger sources, like `kPlusAddressUpdatedInBrowserProcess`, are exempted from paint checks and time resets, as defined in `kTriggerSourcesExemptFromPaintChecks` and `kTriggerSourcesExemptFromTimeReset`. Investigate the security implications of these exemptions, particularly if they bypass security measures or introduce unintended side effects.
        *   **Specific Research Questions:**
            * What are the security implications of exempting trigger sources from paint checks and time resets?
            * Could these exemptions bypass security measures or introduce unintended side effects that could be exploited?
            * Are there any scenarios where malicious actors could manipulate trigger sources to bypass security checks due to these exemptions?
    *   **Address Validation:**
        *   **State Name Validation:** State names are validated and canonicalized using `AlternativeStateNameMap::GetCanonicalStateName`. This function uses country-specific data loaded from protobuf files to ensure state names are valid and consistent. Investigate the data loading and update mechanisms for these protobuf files (`AlternativeStateNameMapUpdater`) to ensure data integrity and prevent potential data injection vulnerabilities.
            * **Specific Research Questions:**
                * How secure is the data loading and update mechanism for the protobuf files used in `AlternativeStateNameMapUpdater`?
                * Are there any vulnerabilities in the `AlternativeStateNameMapUpdater` that could allow for data injection or corruption of state name validation data?
                * How frequently are the protobuf files updated, and is this update frequency sufficient to address newly identified state name variations or errors?
        *   **US Zip Code Validation:** US zip codes are validated using `IsValidZip`, which employs a regular expression to check for valid US ZIP and ZIP+4 formats. Review the regex and its usage to ensure it effectively prevents invalid zip codes and potential bypasses.
            *   **Specific Research Questions:**
                * Is the regular expression used in `IsValidZip` robust enough to prevent bypasses and handle all valid US ZIP and ZIP+4 formats?
                * Are there any edge cases or variations in US zip code formats that the current regex might not cover, potentially leading to validation bypasses?
                * Could maliciously crafted zip codes exploit any weaknesses in the regex to cause unexpected behavior or vulnerabilities?
        *   **Address Rewriting (Normalization):** `AddressRewriter` normalizes addresses using country-specific, rule-based rewriting. While not strict validation, address rewriting enforces formatting conventions and provides implicit validation. Examine the rule resources and rewriting logic in `AddressRewriter` to understand its effectiveness in preventing malformed addresses and potential security implications of incorrect normalization.
            *   **Specific Research Questions:**
                * How effective is `AddressRewriter` in normalizing addresses across different countries and address formats?
                * Are there any scenarios where incorrect address normalization by `AddressRewriter` could lead to security vulnerabilities or data integrity issues?
                * How are the rule resources for `AddressRewriter` maintained and updated to ensure accuracy and prevent malicious rule injection?
        *   **Limited Explicit Validation:** Explicit validation functions are primarily focused on state names and US zip codes. Validation for other address fields (e.g., street address, city, country, non-US postal codes) is less explicit and might rely on implicit validation through formatting and rewriting. Further investigation is needed to understand the overall robustness of address validation and identify potential gaps.
            * **Specific Research Questions:**
                * What types of validation are performed on address fields other than state names and US zip codes?
                * Are there any address fields that lack sufficient validation, potentially leading to vulnerabilities or data quality issues?
                * How can explicit validation be extended to cover more address fields and improve the overall robustness of address validation?

*   **Data Leakage:**
    *   **Accessibility Events (`FireControlsChangedEvent`):** While `FireControlsChangedEvent` itself doesn't directly expose sensitive data, potential data leakage could occur based on screen reader behavior and the content of the AX Tree. Screen readers might query the AX tree for information related to the control, and if the AX tree contains sensitive data (e.g., in suggestion text or attributes), it could be exposed. Further investigation is needed to analyze the AX tree structure for autofill popups and test screen reader behavior to ensure no unintended data leakage.
        *   **Specific Research Questions:**
            * What sensitive data, if any, is included in the AX Tree for autofill popups?
            * How do screen readers interact with the AX Tree in the context of autofill popups, and could this interaction lead to data leakage?
            * Are there any scenarios where malicious actors could exploit accessibility features to extract sensitive data from autofill popups?
            * Analyze the AX tree structure for autofill popups and test screen reader behavior to ensure no unintended data leakage through accessibility events.

*   **Cross-Site Scripting (XSS):**
    *   **Popup & Payment Sheet Display (`AcceptSuggestion`):** 
        *   **Reduced XSS Risk:** While initially identified as a potential XSS risk, further code analysis indicates that the risk of XSS vulnerability via `suggestion.acceptance_a11y_announcement` is **very low**. This is because `suggestion.acceptance_a11y_announcement` is populated with localized strings from Chromium's resource files, not user-controlled input or external data. These resource files are part of the Chromium codebase and are presumably vetted for security.
        *   **Accessibility Announcements:** The `AcceptSuggestion` function in `autofill_popup_controller_impl.cc` uses `suggestion.acceptance_a11y_announcement` for accessibility announcements via `view_->AxAnnounce()`. 
        *   **Mitigation:** The use of localized strings effectively mitigates the XSS risk in this specific case.
        *   **Best Practice Recommendation:** While direct XSS injection via `acceptance_a11y_announcement` seems unlikely, it is still recommended to follow best practices and sanitize all strings passed to `view_->AxAnnounce()` as a precaution, especially for future code changes.
        *   **Specific Research Questions (Updated):**
            * Confirm that `suggestion.acceptance_a11y_announcement` is **never** populated with user-controlled input or data from external sources. (**Confirmed - very low risk**)
            * While the risk is low, are there any theoretical scenarios where vulnerabilities could still arise due to unexpected behavior in localization functions or AX Tree interpretation? (Further investigation recommended, but low priority)
            * As a best practice, should sanitization be implemented for all strings passed to `view_->AxAnnounce()`, even if not strictly necessary in this case? (**Recommended - best practice**)
            * Examine UI updates in `payment_request_sheet_controller.cc` and `AcceptSuggestion` in `autofill_popup_controller_impl.cc` for potential XSS vulnerabilities in other UI display functions (separate research question).

        * **Recommendation (Updated):**
            * While the XSS risk via `suggestion.acceptance_a11y_announcement` is very low, implement sanitization as a best practice for all strings passed to `view_->AxAnnounce()`.
            * Conduct thorough security reviews for any UI updates or changes that involve displaying dynamic content, especially accessibility announcements.
            * Periodically audit localization functions and resource loading mechanisms for potential vulnerabilities (lower priority risk in this specific case).

    *   **Payment Sheet Display (`payment_request_sheet_controller.cc`):** Lack of sanitization of form data when displaying the payment request sheet could lead to XSS vulnerabilities. Ensure that all UI display functions in `payment_request_sheet_controller.cc` that handle user-provided data are properly sanitizing the data to prevent XSS.
        *   **Specific Research Questions:**
            * Are all UI display functions in `payment_request_sheet_controller.cc` properly sanitizing user-provided data to prevent XSS vulnerabilities?
            * Examine UI updates in `payment_request_sheet_controller.cc` for potential XSS vulnerabilities.

*   **Race Conditions:**
    *   **Popup Visibility (`Show()`/`Hide()`):** The `Show()` and `Hide()` methods in `autofill_popup_controller_impl.cc` are susceptible to race conditions from concurrent calls, potentially leading to UI inconsistencies or crashes. Although mitigations like `AutofillPopupHideHelper`, `NextIdleBarrier`, weak pointers, and asynchronous deletion are in place, the interplay between `Show()` and `Hide()` calls, especially across different threads or events, requires thorough examination. The `NextIdleBarrier` and `kIgnoreEarlyClicksOnSuggestionsDuration` are used to prevent accidental suggestion acceptance immediately after popup display, but their robustness under heavy event concurrency should be validated. The `KeyPressObserver` and its `handler_` also play a role in managing keyboard events, and their synchronization with popup visibility states should be assessed for potential race conditions. The comment in the `Show()` function about simplifying the popup lifecycle highlights ongoing concerns about race conditions and the complexity of popup state management.
        *   **Specific Research Questions:**
            * How robust are the mitigations (e.g., `AutofillPopupHideHelper`, `NextIdleBarrier`) against race conditions in `Show()` and `Hide()` under heavy event concurrency?
            * Are there any scenarios where concurrent calls to `Show()` and `Hide()` from different threads could still lead to UI inconsistencies or crashes?
            * How effective is `NextIdleBarrier` in preventing accidental suggestion acceptance, and are there any edge cases where it might fail?
            * Investigate `Show()` and `Hide()` in `autofill_popup_controller_impl.cc` for race conditions and propose more robust synchronization mechanisms if needed.
    * Analyze thread safety and synchronization mechanisms in popup visibility handling and propose improvements if needed.
        *   **Specific Research Questions:**
            * How thread-safe are the current popup visibility handling mechanisms?
            * Are there any areas where thread safety or synchronization could be improved to prevent race conditions and ensure UI consistency?
            * What are the best practices for thread safety and synchronization in UI handling to prevent race conditions?
            * Analyze thread safety and synchronization mechanisms in popup visibility handling and propose improvements.

*   **Keyboard Input Handling Security:**
    *   Conduct a security audit of `HandleKeyPressEvent` in `autofill_popup_controller_impl.cc` for secure keyboard input handling, focusing on unhandled key events and special key combinations.
        *   **Specific Research Questions:**
            * Have all keyboard events and special key combinations been thoroughly audited for secure handling in `HandleKeyPressEvent`?
            * Are there any unhandled key events or special key combinations that could lead to security vulnerabilities or unexpected behavior?
            * How can keyboard input handling in `HandleKeyPressEvent` be further hardened to prevent potential vulnerabilities?
            * Conduct a security audit of `HandleKeyPressEvent` and implement more robust keyboard input handling mechanisms.

*   **`NextIdleBarrier` and Popup Interaction Logic:**
    *   Review the effectiveness of `NextIdleBarrier` in `Show()` for preventing accidental clicks and ensure thread safety in its implementation.
        *   **Specific Research Questions:**
            * How effective is `NextIdleBarrier` in preventing accidental clicks on suggestions, and are there any scenarios where it might fail?
            * Is the implementation of `NextIdleBarrier` thread-safe and robust against race conditions?
            * Are there any alternative or complementary mechanisms to `NextIdleBarrier` that could further reduce accidental clicks?
            * Review the effectiveness and thread safety of `NextIdleBarrier` and explore alternative mechanisms for preventing accidental clicks.
    * Analyze `ShouldLogPopupInteractionShown` for accurate logging of popup interactions and identify any potential inconsistencies.
        *   **Specific Research Questions:**
            * How accurate is the logging of popup interactions in `ShouldLogPopupInteractionShown`, and are there any potential inconsistencies or inaccuracies?
            * Could inaccuracies in popup interaction logging lead to security-relevant issues or hinder security analysis?
            * How can popup interaction logging be improved for accuracy and reliability?
            * Analyze `ShouldLogPopupInteractionShown` for accurate logging of popup interactions and identify improvement opportunities.

*   **Suggestion Filtering and Handling:**
    * Investigate the handling of suggestions with different `filtration_policy` values in `FilterSuggestions` and assess potential security implications.
        * **Specific Research Questions:**
            * How are suggestions with different `filtration_policy` values handled in `FilterSuggestions`, and are there any security implications?
            * Could improper handling of suggestions based on `filtration_policy` lead to vulnerabilities or unexpected behavior?
            * How can suggestion filtering and handling be improved to ensure security and prevent potential policy-related vulnerabilities?
            * Investigate suggestion filtering and handling based on `filtration_policy` and assess potential security implications.
    * Further analyze data sanitization in `AcceptSuggestion()` for injection prevention and ensure robust sanitization of all suggestion data.
        * **Specific Research Questions:**
            * Is data sanitization in `AcceptSuggestion()` sufficient to prevent injection vulnerabilities, and are there any areas for improvement?
            * Are all types of suggestion data properly sanitized in `AcceptSuggestion()` to prevent injection attacks?
            * What are the best practices for data sanitization in `AcceptSuggestion()` to ensure robust injection prevention?
            * Further analyze data sanitization in `AcceptSuggestion()` and implement more robust sanitization measures.

* **Autofill Prompt Obscuration:**
    * Investigate scenarios where the autofill prompt can be obscured by other UI elements, such as Picture-in-Picture overlays or other dialogs.
    * Analyze the effectiveness of existing occlusion checks and propose improvements to ensure that the autofill prompt is always visible and interactive.

## Code Analysis

### `FilterSuggestions` Function in `autofill_popup_controller_impl.cc`

The `FilterSuggestions` function in `autofill_popup_controller_impl.cc` is responsible for filtering autofill suggestions based on user input. It uses `base::i18n::ToLower` for case-insensitive filtering. The code is as follows:

```cpp
SuggestionFiltrationResult FilterSuggestions(
    const std::vector<Suggestion>& suggestions,
    const AutofillPopupController::SuggestionFilter& filter) {
  SuggestionFiltrationResult result;

  std::u16string filter_lowercased = base::i18n::ToLower(*filter);
  for (const Suggestion& suggestion : suggestions) {
    if (suggestion.filtration_policy ==
        Suggestion::FiltrationPolicy::kPresentOnlyWithoutFilter) {
      continue;
    } else if (suggestion.filtration_policy ==
               Suggestion::FiltrationPolicy::kStatic) {
      result.first.push_back(suggestion);
      result.second.emplace_back();
    } else if (size_t pos = base::i18n::ToLower(suggestion.main_text.value)
                                .find(filter_lowercased);
               pos != std::u16string::npos) {
      result.first.push_back(suggestion);
      result.second.push_back(AutofillPopupController::SuggestionFilterMatch{
          .main_text_match = {pos, pos + filter->size()}});
    }
  }

  return result;
}
```

**Security Analysis:**

The use of `base::i18n::ToLower` provides some level of sanitization by converting the filter string to lowercase, which can help prevent basic injection attempts that rely on case sensitivity. However, it's important to consider whether this is sufficient for preventing more sophisticated injection attacks, especially when dealing with different locales and character sets. 

**Potential Vulnerabilities and Further Research:**

* **Locale-Specific Issues:** Investigate the effectiveness of `base::i18n::ToLower` in different locales. Some locales might have character mappings or transformations that could lead to filtering bypasses or unexpected behavior.
* **Injection Attacks:** Explore if `base::i18n::ToLower` is sufficient to prevent injection attacks. Even with lowercase conversion, certain characters or control sequences might still be interpreted in a way that could lead to vulnerabilities.
* **Regex Filtering (Future):** If regex-based filtering is considered in the future, ensure proper sanitization of regex input to prevent regex injection vulnerabilities.

**Recommendations:**

* **Locale Testing:** Perform thorough testing of `FilterSuggestions` with different locales and character sets to identify any locale-specific vulnerabilities.
* **Input Sanitization:** Consider adding additional input sanitization techniques beyond lowercase conversion to further mitigate potential injection attacks.
* **Regex Security:** If regex filtering is implemented, prioritize security and use robust regex sanitization methods to prevent regex injection vulnerabilities.

**Further Research:**

* Investigate the effectiveness of `base::i18n::ToLower` in sanitizing input against injection attacks across different locales.
* Analyze potential vulnerabilities if regex-based filtering is implemented in the future and how to secure it against regex injection and locale-specific issues.

### `AcceptSuggestion` Function in `autofill_popup_controller_impl.cc`

The `AcceptSuggestion` function handles the acceptance of an autofill suggestion when a user selects it from the popup. It includes a check for `NextIdleBarrier` to mitigate race conditions and prevent accidental clicks immediately after the popup is shown. The code snippet related to `NextIdleBarrier` is as follows:

```cpp
void AutofillPopupControllerImpl::AcceptSuggestion(int index) {
  // ... (other checks and code) ...

  // Ignore clicks immediately after the popup was shown. This is to prevent
  // users accidentally accepting suggestions (crbug.com/1279268).
  if ((!barrier_for_accepting_ || !barrier_for_accepting_->value()) &&
      !disable_threshold_for_testing_) {
    return;
  }

  // ... (rest of the function) ...
}
```

**Security Analysis:**

The `NextIdleBarrier` mechanism is used to prevent accidental suggestion acceptance by ignoring clicks that occur immediately after the popup is displayed. This is a mitigation for potential race conditions where user input might be processed prematurely, leading to unintended actions. The code checks if the `barrier_for_accepting_` is initialized and if its value is true before proceeding with suggestion acceptance. This delay helps ensure that the user intentionally selects a suggestion and reduces the likelihood of accidental selections due to rapid interactions or race conditions.

**Further Research:**

* Analyze the robustness of `NextIdleBarrier` against various race conditions and event concurrency scenarios.
* Investigate potential edge cases where `NextIdleBarrier` might not effectively prevent accidental clicks or introduce new vulnerabilities.
* Explore alternative or complementary mechanisms to further enhance race condition mitigation in popup interaction logic.

### `UpdateFocus` Function in `payment_request_sheet_controller.cc`

The `UpdateFocus` function in `payment_request_sheet_controller.cc` is responsible for updating the focus within the payment sheet UI. It ensures that the focus is correctly set on the intended view, which is important for both usability and security, especially in accessibility contexts. The code is as follows:

```cpp
void PaymentRequestSheetController::UpdateFocus(views::View* focused_view) {
  DialogViewID sheet_id;
  if (GetSheetId(&sheet_id)) {
    internal::SheetView* sheet_view = static_cast<internal::SheetView*>(
        dialog()->GetViewByID(static_cast<int>(sheet_id)));
    // This will be null on first call since it's not been set until CreateView
    // returns, and the first call to UpdateFocus() comes from CreateView.
    if (sheet_view) {
      sheet_view->SetFirstFocusableView(focused_view);
      dialog()->RequestFocus();
    }
  }
}
```

**Security Analysis:**

Proper focus management is crucial for accessibility and can also have security implications. By correctly setting the first focusable view and requesting focus on the dialog, `UpdateFocus` helps ensure that users, including those using screen readers, can navigate the payment sheet UI as intended. This can prevent users from being misled into interacting with unintended elements, which could be exploited in phishing or UI redressing attacks. Ensuring that focus is programmatically set and managed also reduces the risk of focus hijacking or manipulation by malicious scripts.

**Further Research:**

* Analyze potential vulnerabilities related to focus management in the payment sheet UI, such as focus hijacking or manipulation.
* Investigate how focus is handled in different payment sheet states and scenarios to ensure consistent and secure focus behavior.
* Explore best practices for focus management in UI security and accessibility to further enhance the security of the payment sheet UI.

### `FormStructure` Constructor in `form_structure.cc`

The `FormStructure` constructor in `components/autofill/core/browser/form_structure.cc` is responsible for initializing a `FormStructure` object from a `FormData` object. This constructor performs several important steps, including copying form metadata, iterating through form fields, calculating form signatures, and initiating field processing. The code is as follows:

```cpp
FormStructure::FormStructure(const FormData& form)
    : id_attribute_(form.id_attribute()),
      name_attribute_(form.name_attribute()),
      form_name_(form.name()),
      button_titles_(form.button_titles()),
      source_url_(form.url()),
      full_source_url_(form.full_url()),
      target_url_(form.action()),
      main_frame_origin_(form.main_frame_origin()),
      all_fields_are_passwords_(!form.fields().empty()),
      form_parsed_timestamp_(base::TimeTicks::Now()),
      host_frame_(form.host_frame()),
      version_(form.version()),
      renderer_id_(form.renderer_id()),
      child_frames_(form.child_frames()) {
  // Copy the form fields.
  for (const FormFieldData& field : form.fields()) {
    if (!IsCheckable(field.check_status())) {
      ++active_field_count_;
    }

    if (field.form_control_type() == FormControlType::kInputPassword) {
      has_password_field_ = true;
    } else {
      all_fields_are_passwords_ = false;
    }

    fields_.push_back(std::make_unique<AutofillField>(field));
  }

  form_signature_ = CalculateFormSignature(form);
  alternative_form_signature_ = CalculateAlternativeFormSignature(form);
  // Do further processing on the fields, as needed.
  // Computes the `parseable_name_` of the fields by removing common affixes
  // from their names.
  ExtractParseableFieldNames();
  // Computes the `parseable_label_` of the fields by splitting labels among
  // consecutive fields by common separators.
  ExtractParseableFieldLabels();
  SetFieldTypesFromAutocompleteAttribute();
  DetermineFieldRanks();
}
```

**Security Analysis:**

The constructor initializes various attributes of the `FormStructure` object, including form metadata and URLs, which are essential for identifying and processing forms. It iterates through the form fields, counts active fields, and detects password fields. The calculation of `form_signature_` and `alternative_form_signature_` is crucial for form caching and matching, and the subsequent processing steps like `ExtractParseableFieldNames`, `ExtractParseableFieldLabels`, `SetFieldTypesFromAutocompleteAttribute`, and `DetermineFieldRanks` prepare the form structure for further analysis and autofill. While the constructor itself doesn't directly implement explicit security measures, it sets up the foundation for secure form processing by correctly initializing form data and preparing it for subsequent security-relevant operations like heuristic type determination and rationalization.

**Further Research:**

* Analyze the security implications of form signature calculation and potential vulnerabilities related to form signature collisions or manipulation.
* Investigate the security aspects of the field processing steps initiated in the constructor, such as `ExtractParseableFieldNames` and `ExtractParseableFieldLabels`, and identify any potential vulnerabilities in these processing steps.
* Explore how the form metadata and URLs copied in the constructor are used in subsequent security checks and operations within the Autofill component.

## Key Files:

*   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc` - Implementation of the autofill popup controller. Handles user interactions, suggestion display, and input events within the autofill popup UI. Manages popup visibility (`Show`, `Hide`) and uses `NextIdleBarrier` for click prevention.
*   `chrome/browser/ui/autofill/autofill_popup_controller_impl.h` - Header file for `autofill_popup_controller_impl.cc`, defining the interface and public methods of the autofill popup controller.
*   `chrome/browser/ui/autofill/chrome_autofill_client.cc` - Provides the Chromium-specific implementation of the `AutofillClient` interface. Bridges the autofill component with the Chromium browser environment, handling browser-specific functionalities.
*   `chrome/browser/ui/views/payments/payment_request_sheet_controller.cc` - Controller for the payment request sheet UI, handling UI updates, focus management (`UpdateFocus`), button handling, scrolling security (`CanContentViewBeScrollable`), and accessibility features within the payment sheet UI.
*   `components/autofill/core/browser/form_structure.cc` - Core logic for parsing and structuring web forms for autofill. Implements form parsing, field identification, and data extraction to represent form data in a structured format suitable for autofill processing.
*   `components/autofill/core/browser/form_structure.h` - Header file for `form_structure.cc`, defining the data structures and interfaces for representing web forms and their fields in the autofill component.

**Secure Contexts and Privacy:** Autofill should operate securely in HTTPS contexts. Robust privacy measures are essential.

**Vulnerability Note:** High VRP payout history for autofill highlights the need for ongoing security analysis.

The following vulnerabilities related to Autofill have been fixed:

*   Page can obtain autofill data with two consecutive taps using EyeDropper API (bypass of multiple prior fixes) (Commit: 40065604)
*   Autofill prompt can be obscured by Picture-in-Picture overlay, allows stealthy autofill data theft (Commit: 40058582)
*   Autofill prompt can be obscured by FedCM bubble dialog (Commit: 339481295)
*   Pointer lock can be used to bypass mouse movement/keyboard input requirements for autofill (Commit: 40056870)
*   After refactor, page can use EyeDropper API to bypass mouse movement/keyboard input requirements for autofill (regression of issue 1287364) (Commit: 40063230)
*   Page can cause autofill prompt to render near cursor in order to bypass intentional mouse movement input requirements for autofill (Bypass of issue 1240472 fix) (Commit: 40058217)
*   Page can cause autofill prompt to render under cursor in order to bypass mouse movement/keyboard input requirements for autofill (Commit: 40056900)
*   Page can use space key input to cause autofill prompt to render under cursor, bypasses mouse movement/designated keyboard input requirements for autofill (Commit: 40056936)
*   Page can use EyeDropper API to bypass mouse movement/keyboard input requirements for autofill (bypass of issue 1240472 fix) (Commit: 40058496)
