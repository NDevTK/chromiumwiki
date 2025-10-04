# Security Analysis of HTTP Header Parsing

## Overview

The parsing of security-related HTTP headers is a critical function in the browser's security model. Correctly interpreting headers like `Content-Security-Policy`, `Cross-Origin-Embedder-Policy`, and `Cross-Origin-Opener-Policy` is essential for enforcing security policies that protect users from a wide range of attacks. This document analyzes Chromium's approach to parsing these headers, focusing on the central role of `services/network/public/cpp/parsed_headers.cc`.

## Centralized Parsing Logic

Chromium employs a centralized and modular approach to header parsing, with `services/network/public/cpp/parsed_headers.cc` serving as the primary dispatcher. The `PopulateParsedHeaders` function in this file is responsible for orchestrating the parsing of various security-related headers. It iterates through the `HttpResponseHeaders` and calls specialized parsing functions for each header. This design is a strong security practice, as it promotes code reuse, simplifies auditing, and ensures that header parsing is handled consistently across the browser.

## Parsing Strategies

Chromium utilizes two primary strategies for parsing security headers:

1.  **Manual Parsing**: For headers with complex and legacy grammars, such as `Content-Security-Policy`, Chromium employs a manual, character-by-character parsing approach. While this provides fine-grained control, it can be more susceptible to errors if not implemented carefully.

2.  **Structured Header Parsing**: For modern headers like `Cross-Origin-Embedder-Policy` and `Cross-Origin-Opener-Policy`, Chromium leverages the `net::structured_headers` library. This is a significant security enhancement, as it ensures that headers are parsed according to a well-defined and robust specification (RFC 8941). This approach mitigates the risks associated with ambiguous or malformed header values.

## Analysis of Specific Header Parsers

### 1. Content-Security-Policy (CSP)

-   **File**: `services/network/public/cpp/content_security_policy/content_security_policy.cc`
-   **Function**: `AddContentSecurityPolicyFromHeaders`

The parsing of CSP headers is a multi-layered process that reflects the complexity of the CSP specification:

-   **Header Extraction**: The process begins in `AddContentSecurityPolicyFromHeaders`, which extracts all `Content-Security-Policy` and `Content-Security-Policy-Report-Only` headers.
-   **Policy Separation**: `ParseContentSecurityPolicies` then splits comma-separated policies, a feature of HTTP headers that allows multiple policies to be sent in a single header line.
-   **Directive Parsing**: `AddContentSecurityPolicyFromHeader` is the core of the parser. It tokenizes the policy string into directives and their corresponding values. It performs critical validation, such as checking for duplicate directives and ensuring that directives are valid for the given context (e.g., report-only vs. enforced).
-   **Specialized Value Parsers**: Each directive's value is then passed to a specialized parser. For example, `ParseSourceList` handles the complex grammar of source-based directives (like `script-src` or `img-src`), which can include hosts, schemes, keywords (`'self'`, `'unsafe-inline'`), nonces, and hashes.

**Security Implications**: The manual nature of the CSP parser requires careful implementation to avoid parsing bugs that could lead to policy bypasses. However, the modular design, with specialized functions for each part of the grammar, helps to contain complexity and facilitate auditing. The parser's resilience to errors, with detailed logging, is also a key strength.

### 2. Cross-Origin-Embedder-Policy (COEP)

-   **File**: `services/network/public/cpp/cross_origin_embedder_policy_parser.cc`
-   **Function**: `ParseCrossOriginEmbedderPolicy`

The COEP parser is a model of modern, secure header parsing:

-   **Structured Parsing**: It uses `net::structured_headers::ParseItem` to parse the `Cross-Origin-Embedder-Policy` and `Cross-Origin-Embedder-Policy-Report-Only` headers. This eliminates the risk of errors from manually handling the header's syntax.
-   **Token-Based Logic**: The parser recognizes two valid tokens: `require-corp` and `credentialless`. Any other token results in a default, non-restrictive policy (`kNone`), which is a safe failure mode.
-   **Parameter Extraction**: The parser also extracts the `report-to` parameter, which is used for reporting COEP violations.

**Security Implications**: The use of a structured header parser makes the COEP parsing logic robust and easy to verify. The simple, token-based approach minimizes the attack surface.

### 3. Cross-Origin-Opener-Policy (COOP)

-   **File**: `services/network/public/cpp/cross_origin_opener_policy_parser.cc`
-   **Function**: `ParseCrossOriginOpenerPolicy`

Similar to the COEP parser, the COOP parser is modern and secure:

-   **Structured Parsing**: It also uses `net::structured_headers::ParseItem` to parse the `Cross-Origin-Opener-Policy` and `Cross-Origin-Opener-Policy-Report-Only` headers.
-   **Explicit Token Matching**: The parser explicitly checks for the known COOP tokens: `same-origin`, `same-origin-allow-popups`, `unsafe-none`, and the feature-flagged `noopener-allow-popups`. Any unrecognized token is safely ignored.
-   **Feature Flag Control**: The entire policy is gated by the `kCrossOriginOpenerPolicy` feature flag, and its default behavior is controlled by `kCrossOriginOpenerPolicyByDefault`, allowing for careful rollout and management of the feature.

**Security Implications**: The reliance on a structured header parser and explicit token matching makes the COOP parser highly secure and resilient. The use of feature flags adds another layer of control and safety.

## Conclusion

Chromium's header parsing architecture demonstrates a strong commitment to security and robust engineering practices. The centralized dispatcher in `parsed_headers.cc` and the move towards structured header parsing for modern headers are key strengths. The detailed and careful implementation of the CSP parser, while complex, is a testament to the importance of this critical security feature. This analysis provides a solid foundation for understanding how Chromium's security policies are enforced at the network level.