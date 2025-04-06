# Component: Android Intents Handling

## 1. Component Focus
*   **Functionality:** Handles the processing and launching of Android Intents triggered via URL navigations within Chrome for Android, typically using schemes like `intent://` and `android-app://`. This allows web pages to interact with installed Android applications.
*   **Key Logic:** Parsing intent URLs, validating targets and parameters, checking security policies (e.g., preventing navigation to sensitive content providers), interacting with the Android `PackageManager` to resolve and launch intents, handling the intent picker UI.
*   **Core Files:**
    *   `chrome/android/java/src/org/chromium/chrome/browser/externalnav/ExternalNavigationHandler.java`: Core logic for handling external navigations, including intents.
    *   `chrome/android/java/src/org/chromium/chrome/browser/tab/TabRedirectHandler.java`: Handles redirects, potentially involving intents.
    *   `components/external_intents/android/java/src/org/chromium/components/external_intents/`: Shared components for intent handling.
    *   Potentially `IntentUtils.java`, `ExternalNavigationDelegate.java`.

## 2. Potential Logic Flaws & VRP Relevance
*   **Sandbox Escape:** Intents launched from sandboxed iframes bypassing sandbox restrictions (e.g., triggering downloads, navigating top-level context).
    *   **VRP Pattern (`intent://` Sandbox Bypass):** Using `intent://` URLs, potentially nested in `data:` URIs or via popups, to escape iframe sandbox restrictions like `allow-downloads` or `allow-top-navigation`. (VRP: `1365100`; VRP2.txt#7507, #10199, #15706). See [iframe_sandbox.md](iframe_sandbox.md).
*   **Policy Bypass (SameSite):** Using intent navigations (potentially involving redirects or specific intent flags/components) to bypass SameSite cookie restrictions.
    *   **VRP Pattern (SameSite via Intent):** Redirecting to an `intent://` URL which then resolves back to Chrome (e.g., via intent picker choosing Chrome) causing cross-site requests to incorrectly include SameSite=Lax/Strict cookies (VRP: `1375132`, VRP2.txt#10199). See [privacy.md](privacy.md).
*   **Scheme Restriction Bypass:** Circumventing browser restrictions on navigating to specific schemes like `content://`.
    *   **VRP Pattern (`content://` Bypass):** Using `android-app://` URLs targeting specific activities (like Google Quick Search Box) that reflect or re-launch intents, allowing navigation to normally blocked `content://` URIs (VRP2.txt#1865, #8165).
*   **Arbitrary App Launch / Parameter Injection:** Flaws in parsing or validating intent URLs allowing malicious websites to launch unintended apps or inject malicious parameters.
    *   **VRP Pattern (Firebase Dynamic Links):** Using Firebase dynamic links or similar redirectors to mask the final `intent://` URL and potentially bypass origin checks or user confirmation steps (VRP: `40064598`, VRP2.txt#15724).
*   **Information Leaks:** Intent handling potentially leaking sensitive information (e.g., installed apps, file paths) back to the web page.
*   **Denial of Service:** Malformed intents or excessive intent launches causing instability.

## 3. Further Analysis and Potential Issues
*   **Intent URL Parsing:** How robust is the parsing of `intent://` URLs? Are all parameters (package, action, data, extras, flags, component) validated securely?
*   **Sandbox Inheritance:** How are sandbox flags handled when an intent is resolved and potentially launches an activity *within* Chrome (e.g., via Custom Tabs or handling `http/https` intents)? Is the sandbox correctly propagated? (VRP: `1365100`).
*   **Redirect Handling:** How are redirects involving `intent://` URLs handled? Does the security context (initiator origin, user gesture status) remain correct? (VRP: `1375132`).
*   **Scheme Filtering:** How effective are the filters preventing direct navigation to sensitive schemes like `content://`? Can these filters be bypassed via intermediate apps or specific intent structures? (VRP2.txt#1865, #8165).
*   **Intent Picker Logic:** How does Chrome interact with the Android intent picker? Can this interaction be abused (e.g., to force selection or bypass confirmation)?
*   **External App Interaction:** Security implications of launching external apps. Are intents properly sanitized before being passed to the Android system?

## 4. Code Analysis
*   `ExternalNavigationHandler::shouldOverrideUrlLoading()`: Central logic deciding how to handle a navigation URL, including intent schemes.
*   `ExternalNavigationHandler::parseIntent()`: Parses `intent://` URLs. Check for parsing vulnerabilities.
*   `ExternalNavigationHandler::startActivity()`: Launches the resolved intent. Check flags and security context.
*   `ExternalNavigationHandler::hasContentScheme()`: Logic intended to block `content://` navigations (bypassed in VRP2.txt#1865, #8165).
*   `TabRedirectHandler`: Handles redirects; analyze its interaction with intent navigations (VRP: `1375132`).
*   Code related to `android-app://` scheme handling.

## 5. Areas Requiring Further Investigation
*   **Sandbox Escape Vectors:** Thoroughly test intent launching from sandboxed iframes with various flag combinations and intent targets.
*   **SameSite Bypass Scenarios:** Investigate all redirect flows involving `intent://` URLs and subsequent navigations back to web content.
*   **`content://` Bypass:** Re-evaluate the `hasContentScheme` check and other mechanisms intended to block `content://` access. Can reflections via other system apps still bypass this?
*   **Parameter Injection:** Fuzz intent URL parsing with malformed parameters and extras.
*   **Interaction with CCTs:** Analyze security context when an intent resolves to a Custom Chrome Tab.

## 6. Related VRP Reports
*   **Sandbox Bypass:** VRP: `1365100`; VRP2.txt#7507, #10199, #15706
*   **SameSite Bypass:** VRP: `1375132`; VRP2.txt#10199
*   **Scheme Restriction Bypass:** VRP2.txt#1865, #8165 (`content://`), VRP: `1092025` (`android-app://` related).
*   **Origin Check Bypass/Spoofing:** VRP: `40064598` (Firebase Dynamic Links); VRP2.txt#15724 (Firebase Dynamic Links).
*   **App Install Spoof:** VRP: `41493134` (Related to intent handling).

*(See also [iframe_sandbox.md](iframe_sandbox.md), [privacy.md](privacy.md))*