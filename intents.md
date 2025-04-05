# Component: Mobile > Intents

## 1. Component Focus
*   Focuses on how Chromium on Android handles `intent://` URLs.
*   Involves parsing the intent URL, security checks (e.g., restrictions on targets, data), and interaction with the Android Intent system.
*   Relevant files might include:
    *   `components/external_intents/android/java/src/org/chromium/components/external_intents/ExternalNavigationHandler.java`
    *   Code related to URL parsing and Android Intent creation/dispatch.

## 2. Potential Logic Flaws & VRP Relevance
*   Bypassing restrictions on allowed intents or target packages/actions.
*   Exploiting interactions with other mechanisms like Firebase Dynamic Links or redirects to circumvent checks (VRP: 40064598).
*   App install spoofing (VRP: 41493134).
*   Insufficient sandbox inheritance when launched from sandboxed contexts (VRP: 1365100).
*   Origin confusion or incorrect security context when handling intent responses or redirects back to the browser.

## 3. Further Analysis and Potential Issues
*   *(Detailed analysis of intent parsing logic, security checks implemented in `ExternalNavigationHandler`, and interactions with specific Android system behaviors to be added.)*
*   How are `intent://` URLs handled when initiated from different contexts (top frame, iframe, sandboxed iframe)?
*   Can redirects (client-side or server-side) interact dangerously with `intent://` handling?

## 4. Code Analysis
*   *(Specific code snippets related to `hasContentScheme`, intent parsing, and security checks within `ExternalNavigationHandler.java` to be added.)*

## 5. Areas Requiring Further Investigation
*   Explore different parameters and flags within `intent://` URLs for potential bypasses.
*   Investigate the interaction with other URL schemes or external app handlers (e.g., `android-app://`).
*   Security implications of intents launched via bookmarks or history navigation.

## 6. Related VRP Reports
*   VRP #40064598 (P1, $3000): intent:// restrictions bypassed via firebase dynamic links
*   VRP #41493134 (P1, $0): Android app install spoof via intent
*   VRP #1365100: Iframe sandbox allows redirecting to intents, including redirecting to navigation intents (Related, potentially duplicates/expands on sandbox bypass aspect)

*(This list should be reviewed against VRP.txt/VRP2.txt for completeness regarding Intent handling reports).*