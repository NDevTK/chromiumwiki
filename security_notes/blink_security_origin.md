# Blink SecurityOrigin (`third_party/blink/renderer/platform/weborigin/security_origin.h`)

## 1. Summary

The `SecurityOrigin` class is the in-renderer implementation of the Same-Origin Policy (SOP), arguably the most fundamental security boundary on the web. It is the primary mechanism within the Blink rendering engine that prevents documents and scripts from different origins from accessing each other's data and DOM.

Every execution context (e.g., a `LocalDOMWindow`) has a `SecurityOrigin`. Before any cross-origin action is attempted—such as accessing a property on another window, fetching a resource, or sending a postMessage—Blink consults this class to determine if the action is permitted. A logical flaw in this class would lead to a universal cross-site scripting (UXSS) vulnerability, completely compromising the web's security model.

## 2. Core Concepts

The class models the concept of an "origin" as defined by the HTML Standard.

*   **Tuple Origins:** The standard representation of an origin, consisting of a `(scheme, host, port)` tuple. Two tuple origins are considered "same-origin" only if all three components are identical.

*   **Opaque Origins:** A unique, non-deterministic origin that is unequal to all other origins, including itself.
    *   **Purpose:** Opaque origins are assigned to content that lacks a natural origin, such as `data:` URLs, documents created from `blob:` URLs, or sandboxed iframes. This ensures that such content is fully isolated and cannot access any other content.
    *   **Mechanism:** Opaque origins are backed by a `nonce` (a unique, unguessable token) to guarantee their uniqueness.
    *   **Precursor Origin:** An opaque origin can have a "precursor," which is the tuple origin that created it (e.g., the origin of the document that created the sandboxed iframe). This precursor information is used for certain policy decisions (like CSP) but is **never** used for security checks.

*   **`document.domain`:** A legacy feature that allows origins on different subdomains to relax the SOP. For example, `https://a.example.com` and `https://b.example.com` can both set `document.domain = "example.com"` to gain limited access to each other. The `SecurityOrigin` class contains the complex logic to handle this relaxation correctly.

*   **Agent Clusters:** A newer web platform feature (`Origin-Agent-Cluster` header) that allows a site to request a higher degree of isolation. Even if two origins are same-origin-domain (via `document.domain`), if they are in different agent clusters, synchronous scripting between them is blocked.

## 3. Security-Critical Logic & Vulnerabilities

*   **The `CanAccess` Method:** This is the primary security gatekeeper. It performs a "same origin-domain" check, which is the most complex type of origin comparison as it must correctly factor in `document.domain` modifications and agent cluster isolation. A bug in this method that incorrectly returns `true` would directly lead to SOP bypass.

*   **`IsSameOriginWith` vs. `IsSameOriginDomainWith`:**
    *   `IsSameOriginWith`: This performs the strict, modern "same-origin" check. It compares the (scheme, host, port) tuple or the opaque nonces. **It does not consider `document.domain`**. The code comments correctly identify this as the right choice for almost all new security checks.
    *   `IsSameOriginDomainWith`: This performs the legacy check that includes the `document.domain` relaxation. Using this method where `IsSameOriginWith` is needed could introduce a vulnerability by incorrectly granting access.

*   **Privilege Granting:**
    *   `GrantUniversalAccess()`: This method effectively disables the Same-Origin Policy for this origin. It is an extremely dangerous capability that should only ever be granted to highly privileged contexts, like internal browser components or extensions with the appropriate permissions. If a web-accessible origin ever gained this privilege, it would be a critical vulnerability.
    *   `GrantLoadLocalResources()`: This grants the ability to load resources from the local filesystem (`file://` URLs). This is another powerful privilege that must be tightly controlled to prevent web content from exfiltrating local user data.

*   **Origin Serialization & Comparison (`ToString`, `ToTokenForFastCheck`):** The process of converting an origin to a string is fraught with peril. If two distinct origins could be serialized to the same string, or if a single origin could be serialized to multiple different strings, it could lead to bypasses in other parts of the browser that use string comparison as a shortcut for a full security check. The file notes a known discrepancy in how `file://` URLs are handled compared to the browser process, highlighting the sensitivity of this logic.

## 4. Related Files

*   `third_party/blink/renderer/core/frame/local_dom_window.h`: The class representing a `window` object, which owns the `SecurityContext`.
*   `third_party/blink/renderer/core/execution_context/security_context.h`: A container class that holds the `SecurityOrigin`, the `ContentSecurityPolicy`, and other security-related state for a document or worker.
*   `third_party/blink/renderer/bindings/core/v8/binding_security.h`: Performs security checks before allowing V8 to access properties on cross-origin objects. It is a primary user of `SecurityOrigin::CanAccess`.
*   `content/browser/child_process_security_policy_impl.h`: The browser-process counterpart that enforces origin separation at the process level. The Blink `SecurityOrigin` is the final line of defense within the renderer process itself.