# Component: Custom Protocol Handler Registration (`registerProtocolHandler`)

## 1. Component Focus
*   **Functionality:** Implements the `navigator.registerProtocolHandler()` method ([Spec](https://html.spec.whatwg.org/multipage/system-state.html#custom-handlers)), allowing web pages to register themselves as handlers for specific URL schemes (e.g., `web+`, `mailto`, `irc`). When the user navigates to a URL with a registered scheme, the browser can direct the navigation to the registered web application's URL template, substituting the target URL for `%s`.
*   **Key Logic:** Handling the `registerProtocolHandler` call in the renderer (`NavigatorContentUtils`), validating the requested scheme (`ProtocolHandlerRegistry::CanSchemeBeOverridden`) and URL template in the browser, storing registrations (`ProtocolHandlerRegistry`), prompting the user for permission (`ProtocolHandlerPermissionContext`), intercepting navigations to registered schemes (`ExternalProtocolHandler`), and rewriting the URL based on the registered handler's template (`ProtocolHandler::TranslateUrl`).
*   **Core Files:**
    *   `third_party/blink/renderer/core/navigator/navigator_content_utils.cc`: Implements `NavigatorContentUtils::RegisterProtocolHandler`.
    *   `components/custom_handlers/`: Core logic for protocol handler registration and management.
        *   `protocol_handler_registry.cc`/`.h`: Stores and manages registered handlers. Contains scheme validation logic (`IsSchemeAllowed`, `CanSchemeBeOverridden`).
        *   `protocol_handler.cc`/`.h`: Represents a single handler registration. Contains URL translation logic (`TranslateUrl`).
    *   `chrome/browser/custom_handlers/`: Chrome-specific integration and UI.
        *   `protocol_handler_registry_factory.cc`
        *   `protocol_handler_permission_context.cc` (Handles permission prompts).
    *   `chrome/browser/external_protocol/external_protocol_handler.cc`: Handles navigations to external/unknown protocols, checks for registered web handlers (`ProtocolHandlerRegistry::GetHandlerFor`) before falling back to OS handlers.

## 2. Potential Logic Flaws & VRP Relevance
*   **Scheme/URL Validation Bypass:** Insufficient validation of the `scheme` or the `%s` URL template provided to `registerProtocolHandler`, allowing registration of disallowed schemes (e.g., `file`) or crafting templates that lead to unintended URL constructions or XSS when invoked.
    *   **VRP Pattern (Local File Read via `file:` Scheme):** A historical vulnerability allowed registering a handler for a scheme (potentially bypassing the blocklist) with a handler URL template that, when invoked with a crafted target URL, resulted in the handler page being able to access `file://` resources. This likely involved bypassing scheme validation in `CanSchemeBeOverridden` or flaws in how the final translated URL was handled regarding origin checks. (VRP: `971188`). See [file_system_access.md](file_system_access.md).
    *   **VRP Pattern (`registerProtocolHandler` bypass - General):** General bypasses related to the API's restrictions (VRP2.txt#12036 - details unknown).
*   **Permission Prompt Spoofing/Bypass:** Misleading the user in the permission prompt (e.g., hiding the true handler URL or scheme), or bypassing the need for user consent altogether.
*   **Origin/Security Context Confusion:** The handler URL being opened in an incorrect security context or with incorrect origin information, potentially leading to SOP bypasses or other security issues.
*   **Cross-Site Scripting (XSS) via URL Template:** If the handler URL template or the parameters substituted into it (`%s`) are not properly sanitized/encoded by the *handler page itself*, it could lead to XSS in the context of the registered handler page when invoked with a malicious target URL. (This is primarily a risk for the handler application, not Chrome itself, unless Chrome's substitution logic is flawed).
*   **Interaction with External Protocol Dialogs:** While `registerProtocolHandler` handles *web-based* handlers, the browser also shows dialogs for launching external OS handlers. Flaws in navigation or redirects could cause these external dialogs to show a spoofed origin.
    *   **VRP Pattern (External Dialog Origin Spoof via Redirect):** A server-side redirect from `https://originA.com` to `tel:12345` or `customscheme:foo` could cause the external protocol confirmation dialog to incorrectly display `originA.com` as the initiator, rather than the redirecting URL's origin. `registerProtocolHandler` itself isn't directly involved here, but it relates to the broader handling of non-http(s) scheme navigations. (VRP: `40055515`; VRP2.txt#9087, #13592, #11016, #13605, #13555, #15881). See [navigation.md](navigation.md).

## 3. Further Analysis and Potential Issues
*   **Scheme Validation (`IsSchemeAllowed`, `CanSchemeBeOverridden`):** Re-audit the scheme blocklist. Are schemes like `file`, `javascript`, `data`, `blob`, `filesystem`, `chrome`, etc., comprehensively blocked? Can variations (e.g., case differences, URL encoding) bypass the checks? How are `web+` schemes specifically validated? (Related to VRP: `971188`).
*   **URL Template Validation:** How is the `%s` template URL validated (`ProtocolHandler::IsValid`, `GURL::is_valid`)? Are potentially dangerous schemes (e.g., `javascript:`) blocked *within* the template URL itself? Is the substitution logic (`ProtocolHandler::TranslateUrl`) secure against template injection if the handler URL itself contains unexpected characters near `%s`?
*   **Navigation Interception (`ExternalProtocolHandler`):** How are navigations to potentially registered schemes intercepted? Does `ProtocolHandlerRegistry::GetHandlerFor` correctly match schemes? Does this occur before or after other security checks (e.g., Safe Browsing)? Can this interception be bypassed?
*   **Permission Handling (`ProtocolHandlerPermissionContext`):** Review the permission prompt UI and logic. Is the requesting origin and target handler URL clearly and accurately displayed? Can the prompt be spoofed or bypassed?
*   **URL Rewriting (`TranslateUrl`):** Analyze the logic that substitutes the target URL into the `%s` template. Ensure proper escaping/encoding prevents injection attacks if the target URL contains special characters.

## 4. Code Analysis
*   `NavigatorContentUtils::RegisterProtocolHandler`: Renderer-side API entry point, performs initial scheme validation (`IsProtocolHandlerSchemeAllowed`). Sends IPC to browser.
*   `ProtocolHandlerRegistry`: Browser-side registry. Stores handlers (`RegisterHandler`, `OnProtocolHandlerRegister`, persisted in Prefs). Checks scheme validity (`IsSchemeAllowed`, `CanSchemeBeOverridden`). Looks up handlers (`GetHandlerFor`).
*   `ProtocolHandler`: Represents a registered handler. Contains URL translation logic (`TranslateUrl`, uses `ReplaceStringPlaceholders`). Validates template URL (`IsValid`).
*   `ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck`: Handles launching external URLs. Calls `GetHandlersFor`, potentially rewriting URL via `TranslateUrl` if a web handler is found. **Name is concerning - need to verify sufficient checks happen *before* this point.**
*   `ProtocolHandlerPermissionContext`: Handles user permission prompts for registration.

## 5. Areas Requiring Further Investigation
*   **Scheme Validation Robustness:** Test registration with various potentially sensitive schemes (`file`, `javascript`, etc.) and edge cases (non-standard characters, long schemes, case variations, URL encoding). (Related to VRP: `971188`).
*   **URL Template Security:** Test with template URLs containing different schemes (`javascript:`?), path traversal attempts, or characters that might break parsing upon substitution (`TranslateUrl`).
*   **Navigation Interception Logic:** Trace the flow from URL navigation (`NavigateURL`, `ExternalProtocolHandler`) to `ProtocolHandlerRegistry::GetHandlerFor` and invocation of a registered handler (`TranslateUrl`). Look for bypasses or race conditions.
*   **Interaction with other Navigation Features:** How does this interact with Site Isolation, Service Workers, redirects, sandboxed iframes, etc.? Can a registered handler be invoked in an unexpected security context?

## 6. Related VRP Reports
*   VRP: `971188` (Local file read via registerProtocolHandler - likely `file:` scheme bypass)
*   VRP2.txt#12036 (Protocol handler bypass - details unknown)
*   *(Indirectly related)* VRP: `40055515`; VRP2.txt#9087, #13592, #11016, #13605, #13555, #15881 (Origin spoofing in *external* protocol dialogs via redirects).

## 7. Cross-References
*   [navigation.md](navigation.md)
*   [file_system_access.md](file_system_access.md)
*   [permissions.md](permissions.md)