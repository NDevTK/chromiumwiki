# Security Note: `services/network/public/cpp/content_security_policy/content_security_policy.cc`

## Overview

This file contains the core logic for parsing and handling Content Security Policy (CSP) in the Chromium network service. It is responsible for taking raw CSP headers, parsing them into a structured format, and providing the mechanisms to check if a given request is allowed by the policy. This file is critical to the enforcement of CSP, a key web security feature that helps mitigate cross-site scripting (XSS) and other content injection attacks.

## Parsing

The primary entry points for parsing are `ParseContentSecurityPolicies` and `AddContentSecurityPolicyFromHeader`. These functions handle the `Content-Security-Policy` and `Content-Security-Policy-Report-Only` HTTP headers.

The parsing logic is quite detailed and follows the W3C specification for CSP. Here are some key aspects:

-   **Header Splitting**: The code correctly handles multiple CSPs in a single header, separated by commas.
-   **Directive Parsing**: `ParseHeaderValue` splits the header into individual directives. Each directive is then parsed based on its specific grammar.
-   **Source List Parsing**: `ParseSourceList` is a crucial function that parses the source expressions for directives like `script-src`, `style-src`, etc. It handles various source types, including:
    -   Keywords like `'self'`, `'none'`, `'unsafe-inline'`, `'unsafe-eval'`.
    -   Nonces (`'nonce-...'`) and hashes (`'sha256-...'`).
    -   Hosts, schemes, and paths.
-   **Error Handling**: The parser is designed to be robust against malformed headers. It collects parsing errors and reports them, but will still try to enforce the valid parts of the policy.

## Directive Handling

The file defines and handles a wide range of CSP directives. The `ToCSPDirectiveName` function maps directive names (strings) to an enum, and the `CSPFallbackDirective` function implements the specified fallback logic (e.g., if `script-src` is not present, it falls back to `default-src`).

Security-sensitive directives handled here include:

-   `frame-ancestors`: Prevents clickjacking attacks.
-   `sandbox`: Restricts the capabilities of a document.
-   `upgrade-insecure-requests`: Enforces HTTPS.
-   `require-trusted-types-for`: Enforces Trusted Types.

## Source Matching

The core of CSP enforcement lies in checking if a given URL matches the source list of a directive. This is primarily handled by the `CheckCSPSourceList` function (defined in `csp_source_list.cc` but used here). The `CheckContentSecurityPolicy` function orchestrates this check, taking into account the directive, the URL, and the context of the request.

Key aspects of the source matching logic include:

-   **'self' Origin**: The concept of the 'self' origin is handled by `ComputeSelfOrigin`, which correctly handles different URL schemes.
-   **Redirects**: The `has_followed_redirect` parameter is used to apply stricter checks for redirected requests.
-   **Bypassing CSP**: The `ShouldBypassContentSecurityPolicy` function provides a mechanism for certain schemes (like `chrome-extension://`) to bypass CSP. This is a critical security boundary.

## Reporting

When a CSP violation occurs, the browser can send a report to a specified endpoint. This file implements the logic for generating these reports.

-   **`ReportViolation`**: This function constructs the violation report, including the blocked URL, the violated directive, and other relevant information.
--   **`report-uri` and `report-to`**: The code supports both the older `report-uri` directive and the newer `report-to` directive (which uses the Reporting API). `ParseReportDirective` handles the parsing of these directives.
-   **Data Sanitization**: `SanitizeDataForUseInCspViolation` is a critical function that sanitizes the data in the violation report to prevent information leaks.

## Security-Critical Functions

-   **`CheckContentSecurityPolicy`**: The main entry point for checking if a request is allowed by a CSP. Any bug in this function could lead to a CSP bypass.
-   **`ParseSourceList`**: Incorrect parsing of source lists could lead to misinterpretation of the policy, potentially allowing blocked resources.
-   **`ShouldBypassContentSecurityPolicy`**: This function defines which requests are exempt from CSP. An overly broad exemption could create a security vulnerability.
-   **`ReportViolation` and `SanitizeDataForUseInCspViolation`**: These functions are critical for preventing information leaks in violation reports.
-   **`UpgradeInsecureRequest`**: This function is responsible for upgrading HTTP requests to HTTPS. A flaw here could undermine the protection of `upgrade-insecure-requests`.

## Interactions

This file is part of the network service and has several important interactions:

-   **`CSPContext`**: This is an interface that provides context for CSP checks, such as the 'self' origin and whether to bypass CSP. The implementation of this interface in the browser process is critical to the overall security of CSP.
-   **Mojom**: The file uses Mojo interfaces (`content_security_policy.mojom`) to communicate with other parts of the browser, such as the renderer.
-   **`net::HttpResponseHeaders`**: The CSPs are extracted from the HTTP response headers.

## Security Researcher Notes

-   The distinction between "enforce" and "report-only" policies is handled throughout the file. A report-only policy will trigger a report but will not block the request.
-   The handling of `frame-ancestors` is particularly important, as it is a key defense against clickjacking.
-   The parsing of nonces and hashes is security-critical. A bug here could allow an attacker to bypass script and style restrictions.
-   The fallback logic for directives is complex and a potential source of bugs. For example, `worker-src` has a different fallback chain than other directives.
-   The file contains several feature flags that enable or disable new CSP features. These flags can be a source of vulnerabilities if not implemented correctly.
-   Pay close attention to how redirects are handled. The URL before redirects is used in some checks, which is an important security consideration.
-   The code has to handle CSPs delivered via HTTP headers and via `<meta>` tags. The set of supported directives is different for each.
-   The `Subsumes` function is used to check if one CSP is a subset of another. This is used for features like `EmbeddedEnforcement` and is security-critical.