# Security Notes: `third_party/blink/renderer/platform/weborigin/scheme_registry.cc`

## File Overview

This file implements the `SchemeRegistry`, a highly security-critical component within the Blink rendering engine. It acts as a central, authoritative database that defines the security properties and policies for every URL scheme (e.g., `http`, `https`, `file`, `chrome-extension`). Code throughout Blink queries this registry to make consistent and reliable security decisions, such as whether a scheme can access local files, bypass Content Security Policy, or, most critically, execute `javascript:` URLs.

## The `javascript:` URL Threat and Defense

`javascript:` URLs are a classic and powerful vector for cross-site scripting (XSS). If an attacker can cause the browser to navigate to a `javascript:` URL (e.g., by setting `window.location.href` or an `<a>` tag's `href`), they can execute arbitrary script in the context of the current origin.

The primary defense against this, implemented via this registry, is to strictly control **which document schemes are allowed to initiate navigations to `javascript:` URLs**. For example, a standard `https` page should be able to, but a privileged internal `chrome://` page should not, as this could lead to a sandbox escape.

## Key Security Mechanisms and Patterns

### 1. The Denylist: `not_allowing_javascript_urls_schemes`

The core of the `javascript:` URL defense is a simple but effective blocklist.

-   **Mechanism**: The file maintains a global `URLSchemesSet` called `not_allowing_javascript_urls_schemes`.
-   **`RegisterURLSchemeAsNotAllowingJavascriptURLs(scheme)`**: This function adds a scheme to the blocklist. This is the security-critical operation where a scheme is declared as being forbidden from executing `javascript:` URLs.
-   **`ShouldTreatURLSchemeAsNotAllowingJavascriptURLs(scheme)`**: This is the query function that enforces the policy. Code elsewhere in Blink (e.g., in the navigation logic) calls this function with the scheme of the source document. If it returns `true`, the navigation to the `javascript:` URL is blocked.

This centralized, "deny by default" (for registered schemes) model is a much stronger security posture than scattering individual checks throughout the codebase.

### 2. Immutable-After-Startup Security Policies

A cornerstone of this component's security model is that all security policies are intended to be immutable after browser startup.

-   **`GetMutableURLSchemesRegistry()`**: This function, which allows modification of the scheme lists, contains a critical assertion: `DCHECK(IsBeforeThreadCreated())`.
-   **Security Implication**: This is a powerful security guarantee. It enforces that all security policies must be registered during the browser's single-threaded startup phase. Once multi-threading begins, the policies are effectively "baked in" and cannot be changed. This completely eliminates a wide class of vulnerabilities related to race conditions, where one thread might be checking a policy while another is modifying it.
-   **Testing**: The file provides a `GetMutableURLSchemesRegistryForTest()` function that bypasses this check, explicitly acknowledging that runtime modification is only safe and permissible in a controlled testing environment.

### 3. Centralized Registry for All Scheme-Based Policies

The security value of this file extends far beyond just `javascript:` URLs. It serves as the central authority for numerous other critical security policies, demonstrating a strong architectural pattern. Other policies managed here include:
-   **CORS Enabled Schemes**: Determines which schemes are subject to Cross-Origin Resource Sharing checks.
-   **CSP Bypassing Schemes**: A highly privileged list of schemes (like `chrome-extension://`) that are exempt from Content Security Policy.
-   **Secure Contexts**: Schemes that are considered secure contexts even if they aren't served over HTTPS.
-   **Domain Relaxation**: Schemes for which the legacy `document.domain` security-weakening feature is forbidden.

## Summary of Security Posture

The `SchemeRegistry` is a foundational and well-designed security component in Blink.

-   **Security Model**: It is built on the principle of a centralized, immutable-after-startup policy database. This provides a high degree of assurance against race conditions and inconsistent security policy enforcement.
-   **Primary Risks**: The security of the system depends on the **correctness of the initial registration**.
    -   A scheme that should be restricted but is never registered with `RegisterURLSchemeAsNotAllowingJavascriptURLs` would create a security hole.
    -   Any logic that allows a non-test code path to get access to the mutable registry after startup would be a critical vulnerability.
-   **Audit Focus**: A security review should focus not on this file itself, but on its **callers**. The primary audit task is to verify that all custom schemes and internal browser schemes are correctly registered with their appropriate security policies (especially the `javascript:` URL restriction) during the browser's startup sequence.