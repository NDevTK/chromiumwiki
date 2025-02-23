# Content Security Policy (CSP)

This page documents potential security vulnerabilities related to Content Security Policy (CSP) in Chromium. CSP is a crucial security mechanism that helps mitigate cross-site scripting (XSS) and other code injection attacks.  The VRP data highlights the importance of secure CSP handling.  **Note:** CSP Level 1 is obsolete; refer to CSP Level 2 and beyond.

## Potential Vulnerabilities:

* **Policy Parsing and Enforcement:** Incorrect parsing or enforcement of CSP directives could allow bypasses.  The parsing and enforcement logic in `content_security_policy.cc` needs thorough review.
* **Nonce and Hash Verification:**  Incorrect nonce or hash verification for inline scripts and styles could allow malicious code execution.  The `AllowInline` function is critical for secure nonce/hash handling.
* **Request Handling:** Weaknesses in request handling based on CSP could allow unauthorized requests.  The `AllowRequest` function and its interaction with different request contexts and destinations need careful analysis.
* **Bypass Techniques:**  Attackers might discover bypass techniques for CSP, either through flaws in the implementation or by exploiting interactions with other browser features.  Regular testing and analysis are crucial to identify and mitigate bypasses.
* **Unsafe Directives:**  The use of unsafe directives like `unsafe-inline` or `unsafe-eval` weakens CSP and should be avoided if possible.  The handling of these directives in `csp_directive_list.cc` needs careful review.
* **Dangling Markup Injection:**  Dangling markup injection could allow attackers to bypass CSP by injecting malicious code into existing nonced elements.  The `IsNonceableElement` function is crucial for preventing this type of attack.
* **Unicode and IDN Hostname Vulnerabilities:** CSP source matching might be vulnerable to bypasses due to improper handling of Unicode and Internationalized Domain Names (IDN) in hostnames. 
    * **Unicode Homograph Attacks:** Attackers could use Unicode characters that look similar to ASCII characters (homographs) to create malicious hostnames that bypass CSP policies. For example, using the Cyrillic 'а' (U+0430) instead of the ASCII 'a' (U+0061) in a hostname.
        * **Example:** A policy like `img-src https://example.com;` might be bypassed by a URL like `https://exаmple.com/malicious.jpg` if Unicode homographs are not correctly handled.
    * **IDN Punycode Bypasses:** Inconsistent handling of Punycode and Unicode representations of IDN hostnames could lead to bypasses. If CSP policies use Unicode hostnames but URLs use Punycode, or vice versa, matching might fail, leading to vulnerabilities.
        * **Example:** A policy like `img-src https://例え.com;` (Unicode) might be bypassed by a URL like `https://xn--r8jz45g.com/malicious.jpg` (Punycode) if Punycode conversion is not consistently applied in source matching.
    * **Wildcard Matching Issues:** Wildcard matching with Unicode/IDN hostnames (`*.例え.com`) might have unexpected behavior or edge cases, potentially leading to overly permissive or restrictive policies.

    **Recommendation:** Thoroughly review the `HostMatches` function in `csp_source.cc` and related URL parsing code to ensure proper Unicode normalization, homograph detection (if applicable), and consistent handling of Punycode and Unicode hostnames in CSP source matching. Add comprehensive test cases for Unicode and IDN hostnames to `csp_source_test.cc` to verify secure and correct behavior.

## Further Analysis and Potential Issues:

* **`third_party/blink/renderer/core/frame/csp/content_security_policy.cc`:** The `ContentSecurityPolicy` class handles CSP. Key functions include `AddPolicies`, `AllowInline`, `AllowRequest`, `AllowEval`, `AllowWasmCodeGeneration`, `ReportViolation`, `EnforceSandboxFlags`, `RequireTrustedTypes`, `EnforceStrictMixedContentChecking`, `UpgradeInsecureRequests`, `ShouldBypassContentSecurityPolicy`, `HasPolicyFromSource`, and `AllowFencedFrameOpaqueURL`.
* **`third_party/blink/renderer/core/frame/csp/csp_directive_list.cc`:** This file contains complex logic for handling various CSP directives, including `script-src`, `object-src`, `style-src`, etc.  Functions like `CSPDirectiveListAllowFromSource`, `CSPDirectiveListAllowInline`, `CSPDirectiveListAllowEval`, and `CSPDirectiveListAllowHash` need a thorough security audit.
* **CSP Level 2+ Features:**  The implementation of CSP Level 2 and later features, such as new directives, reporting mechanisms, and stricter parsing rules, needs careful review to ensure they are secure and effective.
* **Interaction with Other Security Mechanisms:**  The interaction between CSP and other security mechanisms, such as CORS, HTTPS, and sandboxing, should be analyzed for potential conflicts or bypasses.

## Areas Requiring Further Investigation:

* **Nonce Generation and Management:**  The generation and management of nonces for inline scripts and styles should be reviewed to ensure uniqueness and prevent reuse.
* **Hash Algorithm Security:**  The security of the hash algorithms used for whitelisting resources should be evaluated, considering potential collision attacks or weaknesses in the algorithms themselves.
* **Reporting Mechanism Security:**  The security of the reporting mechanisms, including the handling of report-uri and report-to directives, needs further analysis to prevent information leakage or bypasses.
* **Parser and Enforcement Interaction:**  The interaction between the CSP parser and the enforcement logic should be reviewed for potential inconsistencies or vulnerabilities.
* **Strict Dynamic:**  The implementation and security implications of the 'strict-dynamic' source expression need further analysis, especially regarding its interaction with nonces and hashes.

## Files Reviewed:

* `third_party/blink/renderer/core/frame/csp/content_security_policy.cc`
* `third_party/blink/renderer/core/frame/csp/csp_directive_list.cc`
