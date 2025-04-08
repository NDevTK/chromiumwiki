# Component: Custom Protocol Handler Registration (`registerProtocolHandler`)

## 1. Component Focus
*   **Functionality:** Implements the `navigator.registerProtocolHandler()` method ([Spec](https://html.spec.whatwg.org/multipage/system-state.html#custom-handlers)), allowing web pages to register themselves as handlers for specific URL schemes (e.g., `web+`, `mailto`, `irc`). When the user navigates to a URL with a registered scheme, the browser can direct the navigation to the registered web application.
*   **Key Logic:** Handling the `registerProtocolHandler` call, validating the requested scheme and URL template, storing registrations (`ProtocolHandlerRegistry`), prompting the user for permission, intercepting navigations to registered schemes (`ExternalProtocolHandler`), and rewriting the URL based on the registered handler's template.
*   **Core Files:**
    *   `third_party/blink/renderer/core/navigator/navigator_content_utils.cc`: Implements `NavigatorContentUtils::RegisterProtocolHandler`.
    *   `components/custom_handlers/`: Core logic for protocol handler registration and management.
        *   `protocol_handler_registry.cc`/`.h`: Stores and manages registered handlers.
        *   `protocol_handler.cc`/`.h`: Represents a single handler registration.
    *   `chrome/browser/custom_handlers/`: Chrome-specific integration and UI.
        *   `protocol_handler_registry_factory.cc`
        *   `protocol_handler_permission_context.cc` (Handles permission prompts).
    *   `content/browser/renderer_host/render_frame_host_impl.cc`: Potentially involved in intercepting navigations (`DecidePolicyForNavigation`).
    *   `chrome/browser/external_protocol/external_protocol_handler.cc`: Handles navigations to external/unknown protocols, potentially interacting with registered handlers.

## 2. Potential Logic Flaws & VRP Relevance
*   **Scheme/URL Validation Bypass:** Insufficient validation of the `scheme` or the `%s` URL template provided to `registerProtocolHandler`, allowing registration of disallowed schemes (e.g., `file`) or crafting templates that lead to unintended URL constructions when invoked.
    *   **VRP Pattern (Local File Read):** A vulnerability allowed registering a handler that, when invoked, could be used to read local files, likely due to improper validation or URL construction allowing access to `file://` URLs (VRP: `971188`). See [file_system_access.md](file_system_access.md).
    *   **VRP Pattern (`registerProtocolHandler` bypass):** General bypasses related to the API (VRP2.txt#12036).
*   **Permission Prompt Spoofing/Bypass:** Misleading the user in the permission prompt, or bypassing the need for user consent altogether.
*   **Origin/Security Context Confusion:** The handler URL being opened in an incorrect security context or with incorrect origin information, potentially leading to SOP bypasses.
*   **Cross-Site Scripting (XSS):** If the handler URL template or the parameters substituted into it (`%s`) are not properly sanitized/encoded, it could lead to XSS in the context of the registered handler page.

## 3. Further Analysis and Potential Issues
*   **Scheme Validation:** Analyze the validation logic in `NavigatorContentUtils::RegisterProtocolHandler` and `ProtocolHandlerRegistry::CanSchemeBeOverridden`. Are schemes like `http`, `https`, `file`, `javascript`, `data`, `blob`, `filesystem`, `chrome`, etc., correctly blocked? How are `web+` schemes handled? (VRP: `971188`).
*   **URL Template Validation:** How is the `%s` template URL validated? Are potentially dangerous schemes blocked within the template? Is the substitution logic secure?
*   **Navigation Interception (`ExternalProtocolHandler`):** How are navigations to potentially registered schemes intercepted? Does this occur before or after other security checks? Can this interception be bypassed?
*   **Permission Handling (`ProtocolHandlerPermissionContext`):** Review the permission prompt UI and logic. Is the requesting origin and target handler clearly displayed?
*   **URL Rewriting:** Analyze the logic (`ProtocolHandler::TranslateUrl`) that substitutes the target URL into the `%s` template. Ensure proper escaping/encoding prevents injection attacks.

## 4. Code Analysis
*   `NavigatorContentUtils::RegisterProtocolHandler`: Renderer-side API entry point, performs initial validation.
*   `ProtocolHandlerRegistry`: Browser-side registry, stores handlers, checks scheme validity (`CanSchemeBeOverridden`).
*   `ProtocolHandler`: Represents a registered handler, includes URL translation logic (`TranslateUrl`).
*   `ExternalProtocolHandler::LaunchUrlWithoutSecurityCheck`: Potentially involved in launching registered handlers. Needs careful review of security checks applied before this point.
*   `ProtocolHandlerPermissionContext`: Handles user permission prompts.

## 5. Areas Requiring Further Investigation
*   **Scheme Validation Robustness:** Test registration with various potentially sensitive schemes (`file`, `javascript`, etc.) and edge cases (non-standard characters, long schemes). (VRP: `971188`).
*   **URL Template Security:** Test with template URLs containing different schemes, path traversal attempts, or characters that might break parsing upon substitution.
*   **Navigation Interception Logic:** Trace the flow from URL navigation to `ExternalProtocolHandler` and the invocation of a registered handler. Look for bypasses or race conditions.
*   **Interaction with other Navigation Features:** How does this interact with Site Isolation, Service Workers, redirects, etc.?

## 6. Related VRP Reports
*   VRP: `971188` (Local file read via registerProtocolHandler)
*   VRP2.txt#12036 (Protocol handler bypass)

*(See also [navigation.md](navigation.md), [file_system_access.md](file_system_access.md))*