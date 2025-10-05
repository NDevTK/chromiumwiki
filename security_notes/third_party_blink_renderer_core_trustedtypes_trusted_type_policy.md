# Security Analysis of trusted_type_policy.cc

## 1. Introduction

`third_party/blink/renderer/core/trustedtypes/trusted_type_policy.cc` is a core component of the Trusted Types API, a critical defense mechanism against DOM-based Cross-Site Scripting (XSS) vulnerabilities. This file implements the `TrustedTypePolicy` class, which is responsible for enforcing developer-defined security policies on data before it is passed to sensitive DOM APIs, known as injection sinks.

## 2. Component Overview

The primary class, `TrustedTypePolicy`, represents a single, named policy created by a web application. This policy contains developer-defined functions (`createHTML`, `createScript`, `createScriptURL`) that are responsible for sanitizing or validating input strings. The core responsibility of the `TrustedTypePolicy` class is to execute these developer-defined functions and wrap the resulting "safe" string in a corresponding `TrustedHTML`, `TrustedScript`, or `TrustedScriptURL` object. These objects serve as a signal that the data has been processed by a security policy and is safe to use in a specific DOM context.

## 3. Key Classes and Interactions

-   **`TrustedTypePolicy`**: The central class in this file. It is instantiated by the `TrustedTypePolicyFactory` and holds the logic for a specific policy.
-   **`TrustedTypePolicyFactory`**: Manages the creation and registry of all `TrustedTypePolicy` objects within a given execution context (e.g., a window or worker). It ensures that policy names are unique and provides the entry point for creating new policies.
-   **`trusted_types_util.cc`**: This file contains utility functions that perform the actual checks at the DOM sink level. When an application attempts to assign a string to a sink like `innerHTML`, these utilities intercept the assignment, check if a `TrustedType` object is required, and if so, invoke the appropriate default policy or throw an error.
-   **V8 Bindings**: The `TrustedTypePolicy` is exposed to JavaScript, and its methods (`createHTML`, etc.) are called from developer code. The V8 bindings are responsible for the transition between the JavaScript and C++ worlds.

## 4. Trust Boundaries and Attack Surface

The primary trust boundary enforced by Trusted Types is between untrusted string data and powerful DOM injection sinks. The `TrustedTypePolicy` is the gatekeeper at this boundary.

The attack surface includes:

-   **Policy Implementation**: The security of Trusted Types relies heavily on the quality of the developer-defined policies. A weak or bypassable policy (e.g., an identity function that simply returns its input) renders the protection useless.
-   **DOM Sink Coverage**: The effectiveness of Trusted Types depends on correctly identifying and patching all possible DOM sinks. Any unpatched sink that can lead to script execution represents a bypass.
-   **DOM API Aliasing**: The DOM provides numerous ways to achieve the same goal. An attacker can bypass security checks if not all aliases for a given operation are covered. This was the root cause of the vulnerability discussed below.

## 5. Security History and Known Issues

The analysis of the issue tracker revealed a significant vulnerability that demonstrates the challenges of securing the complex DOM API:

-   **Issue 40058798: "Security: TrustedTypes does not block assignment when modifying existing attribute value via nodeValue/textContent"**: This was a critical bypass of the Trusted Types mechanism. While direct assignments like `element.srcdoc = '...'` were correctly intercepted and required a `TrustedHTML` object, an attacker could bypass this check by modifying the underlying attribute node directly.

    For example, `iframe.attributes.srcdoc.nodeValue = '...'` was not checked. This allowed an attacker to assign a raw string to a sensitive attribute, completely circumventing the Trusted Types policy and leading to XSS.

    The fix involved ensuring that the Trusted Types checks are also applied when an attribute's `nodeValue` or `textContent` is modified. This vulnerability underscores the critical importance of identifying and securing all possible code paths that can modify sensitive data in the DOM.

## 6. Potential Weaknesses and Conclusion

`trusted_type_policy.cc` is a cornerstone of Chromium's defense-in-depth strategy against DOM XSS. It provides a powerful mechanism for applications to lock down their attack surface.

Potential weaknesses include:

-   **Gaps in DOM API Coverage**: As shown by Issue 40058798, the vast and complex DOM API can hide alternative paths to modify data. Any such path that is not covered by Trusted Types checks is a potential bypass.
-   **Bypassable Policies**: The security of the system is ultimately delegated to the web developer's policy functions. An insecure policy provides a false sense of security.
-   **Spec Evolution**: As new DOM APIs are introduced, the Trusted Types specification and its implementation must be updated to cover them, creating a continuous maintenance burden.

In conclusion, `TrustedTypePolicy` is a powerful and essential security feature. However, its implementation requires a deep understanding of the DOM API's intricacies to ensure that all potential injection vectors are covered. The history of bypasses shows that even seemingly obscure DOM manipulation techniques must be considered and secured.