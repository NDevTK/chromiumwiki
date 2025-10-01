# SafeBrowsingBlockingPage (`components/safe_browsing/content/browser/safe_browsing_blocking_page.h`)

## 1. Summary

This class is a concrete implementation of `BaseBlockingPage` and, by extension, `SecurityInterstitialPage`. It is the specific interstitial shown to users when they attempt to navigate to a page flagged by Google's Safe Browsing service as malicious (e.g., malware, phishing, unwanted software) or otherwise harmful (e.g., a billing page).

It is the primary user-facing component of the Safe Browsing feature, responsible for warning the user, explaining the risk, and handling their decision to either retreat to safety or proceed to the dangerous page.

## 2. Core Concepts

*   **Specialized Interstitial:** While `SecurityInterstitialPage` provides the general framework, `SafeBrowsingBlockingPage` adds the specific logic and UI elements needed for Safe Browsing warnings. This includes specific strings, icons, and logic for reporting.

*   **Threat Details Reporting:** A key feature of Safe Browsing is the ability for users to opt into sending detailed reports back to Google when a threat is encountered. These reports help Google discover new threats and improve the service.
    *   This class manages the initiation of this reporting process.
    *   The `threat_details_in_progress_` member tracks whether a report is being generated.
    *   The `FinishThreatDetails()` method is called when the interstitial is closing. It checks the user's reporting preferences (`prefs::kSafeBrowsingExtendedReportingEnabled`) and, if enabled, sends the report.

*   **Interaction with UI Manager:** The blocking page is created and managed by a `BaseUIManager` (or a subclass). It does not control its own lifecycle. The UI Manager decides when to show the page, and the page reports the user's final decision back to the manager.

*   **Handling Multiple Threats:** A single webpage can contain multiple unsafe resources. The blocking page is designed to handle a list of `UnsafeResource`s, presenting a unified interstitial to the user that covers all detected threats on the page.

## 3. Security-Critical Logic & Responsibilities

*   **Informed User Consent:** The most critical security function is to provide a clear, unambiguous, and non-deceptive warning to the user. The design must make the "go back" option the easiest and most obvious choice, while requiring a deliberate action to proceed.

*   **"Proceed Anyway" Enforcement:** The `is_proceed_anyway_disabled_` member is a critical security feature. When true (often enforced by enterprise policy), the option to click through the warning is removed entirely. This class is responsible for enforcing that restriction in the UI. A bug that incorrectly shows the proceed button would violate enterprise security policy.

*   **Privacy of Threat Reports:** The logic in `FinishThreatDetails()` that checks the user's opt-in status before sending a report is privacy-sensitive. A bug here could lead to reports being sent without user consent, leaking browsing information (like the URL of the malicious page) to Google.

*   **UKM/UMA Metrics Collection:** The class is heavily instrumented to collect anonymized metrics about how users interact with the interstitial (e.g., `LogSafeBrowsingInterstitialBypassedUKM`, `LogSafeBrowsingInterstitialShownUKM`). This data is used to evaluate the effectiveness of the warnings and is also privacy-sensitive.

## 4. Key Classes and Methods

*   `SafeBrowsingBlockingPage(...)`: The constructor, which receives all the necessary context from the `BaseUIManager`, including the list of unsafe resources, the controller client, and various service dependencies like `HistoryService` and `TriggerManager`.

*   `OnInterstitialClosing()`: Overrides the base class method. This is where the decision to call `FinishThreatDetails()` is made.

*   `FinishThreatDetails(..., bool did_proceed, ...)`: The core method for handling threat reporting. It packages up information about the threat and the user's interaction and passes it to the `TriggerManager` for sending.

*   `kTypeForTesting`: A static identifier used in tests to distinguish this type of interstitial from others (like SSL interstitials).

## 5. Related Files

*   `components/safe_browsing/content/browser/base_ui_manager.cc`: The class responsible for creating and managing instances of `SafeBrowsingBlockingPage`.
*   `components/safe_browsing/core/browser/threat_details.cc`: The class responsible for collecting the rich details about the environment (e.g., running processes, system information) that are included in a threat report.
*   `components/safe_browsing/core/common/safe_browsing_prefs.cc`: Defines and manages the user preferences that control Safe Browsing behavior, including the crucial extended reporting opt-in.
*   `components/security_interstitials/core/browser/resources/`: The shared resources directory containing the HTML/JS templates, which this class populates with Safe Browsing-specific strings.