# Component: Content Security Policy (CSP)

## 1. Component Focus
*   **Functionality:** Implements the Content Security Policy standard ([Level 3 W3C Rec](https://www.w3.org/TR/CSP3/)), a mechanism to declare security policies for a web page, primarily mitigating XSS and data injection attacks.
*   **Key Logic:** Parsing CSP headers/meta tags (`ContentSecurityPolicy::AddPolicies`), evaluating directives (`script-src`, `style-src`, `img-src`, `connect-src`, `frame-src`, `frame-ancestors`, `form-action`, `report-uri`, `report-to`, `sandbox`, `require-trusted-types`, etc.) against resource loads, inline scripts/styles (nonce/hash checking), dynamic code execution (`unsafe-eval`), and navigation requests. Handling `'strict-dynamic'`, `'unsafe-inline'`, `'self'`, source expressions (schemes, hosts, wildcards). Integration with other features (Service Workers, extensions, special URL schemes).
*   **Core Files:**
    *   `third_party/blink/renderer/core/frame/csp/content_security_policy.cc`/`.h`: Core CSP object attached to `Document`. Key methods: `AllowScriptFromSource`, `AllowStyleFromSource`, `AllowImageFromSource`, `AllowConnectToSource`, `AllowFrameFromSource`, `AllowInline`, `AllowEval`, `ReportViolation`.
    *   `third_party/blink/renderer/core/frame/csp/csp_directive_list.cc`/`.h`: Parses and stores CSP directives from a policy string.
    *   `third_party/blink/renderer/core/frame/csp/csp_source.cc`/`.h`: Handles parsing and matching of source expressions (hosts, schemes, wildcards, nonces, hashes). Includes `HostMatches`, `SchemeMatches`.
    *   `third_party/blink/renderer/core/frame/csp/csp_context.cc`/`.h`: Provides context for CSP checks.
    *   `content/browser/renderer_host/navigation_request.cc`: Enforces frame-src/fenced-frame-src during navigation (`CheckCSPDirectives`).
    *   `//extensions/common/manifest_handlers/csp_info.cc`: Handles CSP specified in extension manifests.

## 2. Potential Logic Flaws & VRP Relevance
*   **Directive Bypass via Scheme/Navigation:** Exploiting how CSP is applied (or not applied) during navigations to specific schemes or in specific contexts.
    *   **VRP Pattern (`about:blank`):** Navigating a frame to `about:blank` incorrectly losing inherited CSP restrictions (VRP: `996741`, VRP2.txt#1924).
    *   **VRP Pattern (`blob:`):** Navigating to `blob:` URLs potentially bypassing the original context's CSP (VRP: `1115628`, VRP2.txt#7831). Using blob URLs opened by cross-origin frames (VRP2.txt#11985).
    *   **VRP Pattern (`filesystem:`):** Navigating to `filesystem:` URLs bypassing CSP (VRP: `1116446`, VRP2.txt#5009).
    *   **VRP Pattern (`javascript:`):** Incorrect handling of `javascript:` URIs, especially in `srcdoc` attributes (VRP: `1006188`) or specific platforms (iOS VRP2.txt#8077), leading to execution despite CSP. Bypassing CSP via javascript: URL + doubly-nested iframe (VRP2.txt#11985).
    *   **VRP Pattern (`about:srcdoc`):** Navigating to `about:srcdoc` potentially bypassing CSP (VRP2.txt#11413). Using `srcdoc` assignment with specific HTML parsing differences (`tree_builder_simulator`) leading to mutation XSS bypassing sanitizers that rely on standard parsing (VRP2.txt#4643, #1185).
*   **Directive Bypass via Feature Interaction:** Exploiting interactions with other browser features.
    *   **VRP Pattern (Service Workers):** Service workers intercepting requests and potentially modifying responses in ways that bypass CSP checks (VRP: `598077`). See [service_workers.md](service_workers.md).
*   **Policy Parsing and Enforcement Flaws:** Errors in parsing directives or enforcing the policy correctly.
    *   **VRP Pattern (Directive Validation):** Insufficient validation or incorrect handling of newer directives like `script-src-elem` / `script-src-attr` allowing bypasses, especially in extensions (VRP: `1288035`, VRP2.txt#589).
    *   **VRP Pattern (`strict-dynamic` Interaction):** Incorrectly allowing `'unsafe-inline'` when `'strict-dynamic'` is present in `default-src` (VRP2.txt#11841). Spec requires `'unsafe-inline'` to be ignored in this case.
    *   **VRP Pattern (Nonce/Hash Bypass):** Potential flaws in nonce generation, verification (`AllowInline`), or hash verification allowing injection. Dangling markup injection into nonced elements.
*   **Source Matching Issues:** Problems in matching source expressions (hosts, schemes, wildcards) against target URLs.
    *   **VRP Pattern (Unicode/IDN):** Improper handling of Unicode homographs or inconsistent Punycode/Unicode handling in host source matching (`HostMatches` in `csp_source.cc`).
*   **Reporting Leaks:** CSP reporting mechanisms (`report-uri`, `report-to`) leaking sensitive information.
    *   **VRP Pattern (Fragment Leak):** Reports incorrectly including the URL fragment (VRP2.txt#15887).
    *   **VRP Pattern (Redirect URL Leak):** CSP reports and error stack traces leaking post-redirect URLs for blocked scripts (VRP2.txt#5947).

    *(Original list items like "Policy Parsing and Enforcement", "Nonce and Hash Verification", "Request Handling", "Bypass Techniques", "Unsafe Directives", "Dangling Markup Injection", "Unicode and IDN Hostname Vulnerabilities" are now integrated into the VRP Patterns above for better context).*

## 3. Further Analysis and Potential Issues:
*   **Parser (`csp_directive_list.cc`):** Thoroughly audit the parsing logic for all directives. How are complex values, edge cases, and newer directives (`script-src-elem`/`attr`) handled? Is the parsing fully spec-compliant regarding interaction rules (e.g., `'strict-dynamic'` ignoring `'unsafe-inline'` VRP2.txt#11841)?
*   **Enforcement (`content_security_policy.cc`):** Review the enforcement logic (`Allow*` methods). Are checks performed at the correct stages of resource loading/execution? How does it interact with `NavigationRequest` checks, especially for schemes like `about:blank`, `blob:`, `filesystem:` where bypasses occurred (VRP: `996741`, `1115628`, `1116446`)?
*   **`'strict-dynamic'` Logic:** Analyze the implementation. Does it correctly ignore host/scheme sources, `'self'`, and `'unsafe-inline'`? How does it interact with nonces/hashes and script propagation?
*   **Nonce/Hash Implementation:** Review nonce generation (is it sufficiently random?), storage, and comparison logic (`AllowInline`). How are hashes calculated and checked (`AllowHash`)? Is dangling markup injection preventable?
*   **Source Matching (`csp_source.cc`):** Re-verify `HostMatches` and `SchemeMatches` against spec requirements, especially for wildcards, ports, redirects, and Unicode/IDN variations. Ensure consistent Punycode handling.
*   **Reporting Security:** Ensure reporting endpoints (`report-uri`, `report-to`) don't leak sensitive cross-origin information beyond what's allowed by the spec (e.g., fragments VRP2.txt#15887, detailed redirect URLs VRP2.txt#5947).
*   **Interaction with other features:** Analyze interactions with Service Workers (VRP: `598077`), Extensions (Manifest V3 CSP, VRP: `1288035`), Sandboxed Iframes, Portals, Fenced Frames, WebUI, Pre-rendering, Back/Forward Cache. How is CSP state propagated and enforced in these complex contexts?

## 4. Code Analysis
*   `ContentSecurityPolicy::AllowScriptFromSource`, `AllowStyleFromSource`, etc.: Core enforcement points. Check conditions and how source lists are evaluated. How do these interact with schemes like `blob:` or `filesystem:`?
*   `ContentSecurityPolicy::AllowInline`: Handles `'unsafe-inline'`, nonces, and hashes. Check verification logic, especially against dangling markup.
*   `ContentSecurityPolicy::AllowEval`: Handles `'unsafe-eval'`.
*   `CSPDirectiveList::Parse`: Entry point for parsing a policy string. Review handling of newer directives (`script-src-elem`, `script-src-attr`) and interaction rules (`'strict-dynamic'`).
*   `CSPSource::HostMatches`, `SchemeMatches`: Core source expression matching logic. Needs review for Unicode/IDN/wildcard edge cases.
*   `ContentSecurityPolicy::ReportViolation`: Builds and sends violation reports. Check for fragment/redirect URL leaks (VRP2.txt#15887, #5947).
*   `NavigationRequest::CheckCSPDirectives`: Enforcement during navigation. How does it handle CSP on `about:blank` / `about:srcdoc`?
*   `HTMLScriptElement::IsAllowedByCsp`: Checks script execution permission.
*   `CSPSourceList::Allow*`: Check functions like `AllowSpecificSource` and how they handle different schemes.
*   `CSPParser`: Underlying parsing logic.

## 5. Areas Requiring Further Investigation:
*   **Spec Compliance:** Thoroughly compare implementation against the latest CSP Level 3 specification, especially regarding `'strict-dynamic'` ignoring `'unsafe-inline'`/`'self'`/hosts (VRP2.txt#11841), reporting details, and handling of newer directives.
*   **Navigation Interaction:** Analyze all navigation scenarios (including redirects, special schemes like `blob:`, `filesystem:`, `about:srcdoc`) to ensure CSP inheritance and enforcement are correct at all stages. Re-test `about:blank` and `blob:` bypasses (VRP: `996741`, `1115628`).
*   **Service Worker Interactions:** How does CSP apply to scripts executed within service workers and requests intercepted by `FetchEvent`? (VRP: `598077`).
*   **Extension Manifest V3 CSP:** Security review of the CSP specified in Manifest V3 and its enforcement, especially regarding `script-src-elem`/`attr` (VRP: `1288035`).
*   **Trusted Types Interaction:** How does `require-trusted-types` directive interact with other directives and enforcement points?
*   **Unicode/IDN Robustness:** Add extensive test cases for various Unicode/IDN hostname scenarios in `csp_source_test.cc`.
*   **Reporting Leaks:** Verify fixes for fragment (VRP2.txt#15887) and redirect URL (VRP2.txt#5947) leaks in reporting.

## 6. Related VRP Reports
*   **Scheme/Navigation Bypass:** VRP: `996741` (`about:blank`), `1115628` (`blob:`), `1116446` (`filesystem:`), `1006188` (`javascript:` in `srcdoc`); VRP2.txt#1924, #7831, #5009, #11413, #8077 (iOS `javascript:`), #1185 (`about:srcdoc`/mXSS), #4643 (mXSS via `srcdoc`), #11985 (`blob:` popup).
*   **Feature Interaction:** VRP: `598077` (Service Worker intercept).
*   **Directive Validation/Parsing:** VRP: `1288035` (`script-src-elem`/`attr` bypass); VRP2.txt#589 (same), VRP2.txt#11841 (`'strict-dynamic'` + `'unsafe-inline'` issue).
*   **Reporting Leaks:** VRP2.txt#15887 (fragment leak), VRP2.txt#5947 (redirect URL leak).

## 7. Files Reviewed:
*   `third_party/blink/renderer/core/frame/csp/content_security_policy.cc`
*   `third_party/blink/renderer/core/frame/csp/csp_directive_list.cc`
*   `third_party/blink/renderer/core/frame/csp/csp_source.cc`