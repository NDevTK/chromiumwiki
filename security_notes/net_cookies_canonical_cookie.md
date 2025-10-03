# Security Notes for `net/cookies/canonical_cookie.cc`

The `CanonicalCookie` class is a foundational component of Chromium's cookie handling architecture. It represents a single, fully-parsed, and validated cookie. All cookies, whether received from a `Set-Cookie` header or loaded from the persistent store, must be converted into this canonical form. This makes the class and its factory methods (`Create`, `CreateSanitizedCookie`, `FromStorage`) a critical security boundary for preventing a wide range of cookie-based attacks.

## Core Security Responsibilities

1.  **Strict Parsing and Validation**: The primary responsibility of `CanonicalCookie` is to ensure that only well-formed and standards-compliant cookies are processed by the browser. This involves rigorous validation of every part of the cookie.

2.  **Attribute Enforcement**: It enforces the semantic rules associated with various cookie attributes, which are essential for security.

3.  **Sanitization**: It sanitizes and canonicalizes cookie data to prevent ambiguity and interpretation errors that could be exploited by attackers.

## Security-Critical Validation Logic

The security of Chromium's cookie mechanism heavily relies on the validation performed within this file:

*   **Name and Value Validation**:
    *   The `Create` methods use `ParsedCookie` to ensure that cookie names and values conform to the character sets defined in RFC6265.
    *   The `kDisallowNonAsciiCookies` feature flag represents a security hardening measure to reject cookies with non-ASCII characters, which can be used in smuggling attacks or cause parsing issues.
    *   The size of cookie attributes is also checked to prevent resource exhaustion attacks.

*   **Domain Attribute (`Domain`)**:
    *   This is one of the most security-critical validations. The code uses `cookie_util::GetCookieDomainWithString` to ensure that a cookie's domain attribute is valid for the URL setting it.
    *   It prevents a site like `example.com` from setting a cookie for a parent domain (`.com`) or a completely unrelated domain (`google.com`).
    *   It correctly handles host-only cookies (where no domain is specified) and performs canonicalization of the domain string.

*   **Path Attribute (`Path`)**:
    *   The `Path` attribute is canonicalized via `cookie_util::CanonPathWithString`. This ensures that a cookie is only sent for requests on its specified path or subpaths, preventing a cookie from being leaked to other parts of a website.

*   **Cookie Prefixes (`__Host-` and `__Secure-`)**:
    *   The `IsCookiePrefixValid` and `IsCanonical` methods enforce the strict rules for these prefixes, providing a powerful defense-in-depth mechanism.
    *   `__Host-` cookies are locked to a specific hostname (no `Domain` attribute), a specific path (`/`), and must be `Secure`. This makes them highly resistant to being overwritten by a less secure application on a subdomain.
    *   `__Secure-` cookies must have the `Secure` attribute.
    *   The code also checks for "hidden prefixes" in cookies with no name, preventing an attacker from setting a cookie like `Value="__Host-foo=bar"` which could be misinterpreted.

*   **Secure Attribute**:
    *   The `Secure` attribute is strictly checked. The logic in `GetAndAdjustPortForTrustworthyUrls` is particularly interesting, as it allows a trustworthy origin (even if currently accessed over HTTP) to set a `Secure` cookie, anticipating that it will be used in a secure context. This is a nuanced but important part of supporting mixed-content sites securely.

*   **Partitioned Attribute (CHIPS)**:
    *   The `IsCookiePartitionedValid` check ensures that the `Partitioned` attribute is only used in conjunction with the `Secure` attribute.
    *   The `CookiePartitionKey` is a fundamental part of the `CanonicalCookie`, ensuring that the isolation provided by CHIPS is baked into the cookie's identity.

## Time and Expiration Security

*   **Clock Skew**: `ParseExpiration` includes logic to compensate for clock differences between the server and the client. This prevents a cookie from expiring prematurely or lasting longer than intended due to clock drift.
*   **Expiration Cap**: `ValidateAndAdjustExpiryDate` enforces a maximum lifetime for cookies (e.g., 400 days). This is a critical privacy and security feature that prevents the creation of "supercookies" that never expire. It also enforces a much shorter lifetime for cookies from insecure schemes.

## `IsCanonical()` - The Security Gatekeeper

The `IsCanonical()` and `IsCanonicalForFromStorage()` methods serve as the final validation gate. A `CanonicalCookie` object that fails these checks is considered invalid and should be rejected. These methods consolidate many of the security checks mentioned above (prefix rules, valid path, etc.) into a single query. The distinction between the two methods allows for a graceful rollout of stricter rules, applying them to new cookies while still being able to load older, slightly non-compliant cookies from the database.

## Potential Areas for Security Research

*   **Parsing Edge Cases**: Look for edge cases in `ParsedCookie` or the canonicalization logic where a crafted `Set-Cookie` header could be interpreted differently by Chromium than by a server or another browser, potentially leading to security bypasses.
*   **Prefix Bypass**: Investigate if any combination of attributes or URL schemes could bypass the cookie prefix enforcement.
*   **Sanitization Gaps**: Analyze the `CreateSanitizedCookie` method for any gaps where it might fail to properly validate or canonicalize an input, especially when called from extensions or other browser-internal components.
*   **Trustworthy Origin Logic**: The logic for handling `Secure` cookies from "trustworthy" but non-cryptographic schemes is complex. Flaws in this logic could potentially allow a non-secure site to set or access a `Secure` cookie under specific circumstances.

In summary, `canonical_cookie.cc` is a dense and highly security-critical file. Its correctness is fundamental to preventing a wide array of attacks, from CSRF and session fixation to information leakage and cross-site tracking.