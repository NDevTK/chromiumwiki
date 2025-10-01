# ContentSecurityPolicy (`third_party/blink/renderer/core/frame/csp/content_security_policy.h`)

## 1. Summary

This class is the core enforcement engine for Content Security Policy (CSP) within the Blink renderer. It is responsible for parsing, storing, and evaluating the set of security policies that a web page declares via HTTP headers (e.g., `Content-Security-Policy`) or `<meta>` tags.

CSP is a critical, multi-faceted defense against Cross-Site Scripting (XSS), data exfiltration, and other common web attacks. The correctness of this class is therefore paramount to web security. A single logical flaw in its enforcement can render a site's entire CSP policy useless.

## 2. Core Concepts

*   **Policy Container:** A `ContentSecurityPolicy` object holds a list of individual policies delivered for a document. When multiple policies are present, it is responsible for enforcing the *most restrictive* intersection of all of them.

*   **Delegate Model:** The class is designed to be agnostic of its environment (e.g., `Document` vs. `WorkerGlobalScope`). It uses a `ContentSecurityPolicyDelegate` interface to query its environment for necessary context (e.g., the security origin of the protected resource) and to perform actions (e.g., report violations, set sandbox flags).

*   **Directive-Based Enforcement:** The public API of the class is organized around methods that check specific CSP directives. For example:
    *   `AllowScriptFromSource(...)` checks `script-src`.
    *   `AllowConnectToSource(...)` checks `connect-src`.
    *   `AllowInline(...)` checks if inline scripts or styles are permitted, often based on nonces or hashes.
    *   `AllowEval(...)` checks if `eval()` and similar functions are disabled by `script-src`.

*   **Violation Reporting:** When a check fails, the class is responsible for:
    *   Logging a detailed message to the developer console.
    *   Dispatching a `SecurityPolicyViolationEvent` to the DOM.
    *   Sending a JSON report to endpoints specified in a `report-uri` or `report-to` directive.

## 3. Security-Critical Logic & Vulnerabilities

A bug in this class is a CSP bypass, which is a high-severity vulnerability. The entire class is security-critical.

*   **Source Matching Logic:**
    *   **Risk:** The most fundamental part of CSP is matching a resource's URL against a list of allowed sources (`CSPSourceList`). A bug in the matching logic (e.g., incorrect handling of wildcards, ports, paths, or redirects) could allow a resource from a malicious origin to be loaded.
    *   **Implementation:** The core matching logic is delegated to helper classes like `SourceListDirective`, but `ContentSecurityPolicy` is the top-level entry point that orchestrates the checks.

*   **Nonce and Hash Validation:**
    *   **Risk:** CSP's nonce- and hash-based mechanisms for allowing inline scripts are a primary defense against XSS. A flaw that allows a nonce to be reused, or that incorrectly calculates or compares a script's hash, would completely undermine this protection.
    *   **Implementation:** The `AllowInline` method is the gatekeeper for this logic, taking the element and its content/nonce as input. It relies on the `CheckHashAgainstPolicy` helper for hash validation.

*   **Bypass Mechanisms:**
    *   **Risk:** The class contains several intentional bypass mechanisms that must be tightly controlled. The `ShouldBypassMainWorldDeprecated` method, for example, allows isolated worlds (like extensions) to be exempt from the page's CSP. If a regular, untrusted script could trick the system into thinking it was in an isolated world, it could bypass CSP entirely.
    *   **Mitigation:** Access to these bypasses is intended to be controlled by the C++ execution context and should not be controllable from web content.

*   **Trusted Types Enforcement:**
    *   **Risk:** This class is responsible for enforcing the `require-trusted-types` and `trusted-types` directives, which are a powerful defense against DOM XSS. A bug in `AllowTrustedTypePolicy` (which checks if a policy can be created) or `AllowTrustedTypeAssignmentFailure` (which checks if a string can be assigned to a sensitive DOM sink) would neutralize the Trusted Types feature.

*   **Fallback and Strictness Logic:**
    *   **Risk:** CSP has complex fallback rules (e.g., if `script-src` is absent, `default-src` is used). It also has the concept of a "strict" policy (`strict-dynamic`). A bug in implementing this fallback or strictness logic could cause the browser to enforce a weaker policy than the developer intended.
    *   **Implementation:** The `enforces_strict_policy_` member tracks whether a sufficiently strict policy is active.

## 4. Related Files

*   `third_party/blink/renderer/core/frame/csp/csp_directive_list.h`: Represents a single parsed CSP header.
*   `third_party/blink/renderer/core/frame/csp/csp_source.h`: Represents a single source expression in a directive's source list.
*   `third_party/blink/renderer/core/execution_context/security_context.h`: The owner of the `ContentSecurityPolicy` object.
*   `services/network/public/cpp/content_security_policy/content_security_policy.cc`: The code in the `NetworkService` that performs the initial parsing of the CSP headers from the HTTP response before they are sent to the renderer.