# WebAuthnRequestSecurityChecker (`content/browser/webauth/webauth_request_security_checker.h`)

## 1. Summary

The `WebAuthRequestSecurityChecker` is a crucial, centralized security component in the browser process. It is responsible for validating all incoming Web Authentication (`WebAuthn`) requests from the renderer process before they are allowed to proceed. Its purpose is to enforce the fundamental security invariants of the WebAuthn specification, ensuring that a malicious or compromised renderer cannot abuse the API to phish users, create credentials for the wrong sites, or violate cross-origin boundaries.

Because the renderer process is untrusted, all security-sensitive parameters of a WebAuthn call (`navigator.credentials.create()` or `get()`) must be re-validated in the browser process. This class is the primary authority for performing those checks.

## 2. Core Security Concepts & Responsibilities

The checker enforces several distinct security policies:

*   **Relying Party (RP) ID Validation:**
    *   **Concept:** The Relying Party ID (RP ID) defines the scope of a credential. It determines which website a credential can be used for. Per the spec, the requested RP ID must be a registrable domain suffix of the caller's origin, or be identical to it.
    *   **Security Goal:** This prevents a malicious website (e.g., `attacker.com`) from requesting a credential for a legitimate website (e.g., `bank.com`). If this check failed, an attacker could trick a user into creating a credential that the attacker's site could then use for authentication.
    *   **Implementation:** `ValidateDomainAndRelyingPartyID` is the core method for this check.

*   **Ancestor Frame Origin Checks:**
    *   **Concept:** WebAuthn operations are highly privileged and should generally not be invokable by a cross-origin iframe. For example, `evil.com` should not be able to embed `bank.com` in an iframe and then trigger a WebAuthn prompt for `bank.com`, as this would be a powerful phishing primitive.
    *   **Security Goal:** To prevent UI confusion and phishing, a WebAuthn request is only allowed if the requesting frame is same-origin with all of its ancestor frames.
    *   **Implementation:** `ValidateAncestorOrigins` walks the frame tree to enforce this policy.

*   **Legacy U2F AppID Validation:**
    *   **Concept:** Before WebAuthn, the FIDO U2F standard used a concept called "AppID" to scope credentials. For backward compatibility, WebAuthn allows certain sites (almost exclusively Google properties) to assert legacy AppIDs.
    *   **Security Goal:** This is a legacy feature and must be tightly controlled. The checker ensures that only origins explicitly authorized to use a given AppID (e.g., via a `.well-known` file) are allowed to do so. It contains special hardcoded checks for Google's own `gstatic.com` AppIDs.
    *   **Implementation:** `ValidateAppIdExtension` performs these checks.

*   **Remote Desktop Override Validation:**
    *   **Concept:** The implementation allows for a `remote_desktop_client_override_origin`, which appears to be a mechanism for remote desktop solutions to perform WebAuthn operations on behalf of a user.
    *   **Security Goal:** This is an extremely dangerous feature, as the override origin is supplied by the untrusted renderer. The checker's comments explicitly call this out as a **SECURITY** risk and state that this override must be validated against an enterprise policy allowlist before it can be trusted. A failure to do so would allow a compromised renderer to impersonate any origin for WebAuthn.

## 3. Security-Critical Logic & Vulnerabilities

*   **RP ID vs. Origin Mismatch:** A bug in `ValidateDomainAndRelyingPartyID` that allows the RP ID to be something other than a suffix of the caller's origin would be a critical logic flaw, enabling phishing and credential spoofing.

*   **Cross-Origin IFrame Bypass:** A flaw in `ValidateAncestorOrigins`, such as failing to check all ancestors or incorrectly evaluating their origins, would enable powerful phishing attacks.

*   **AppID Side-Channel:** The AppID validation logic, especially the remote fetching of `.well-known` files, could be subject to DNS spoofing or other network attacks if not handled carefully (e.g., without requiring HTTPS).

*   **Untrusted Renderer Data:** The most significant systemic risk is a failure to treat all inputs from the renderer as untrusted. The `remote_desktop_client_override_origin` is a prime example. Any parameter coming from the renderer that influences a security decision must be rigorously validated in this class.

## 4. Key Functions

*   `ValidateDomainAndRelyingPartyID(...)`: The core function for checking that the requested credential scope (RP ID) is valid for the calling origin.
*   `ValidateAncestorOrigins(...)`: Enforces the same-origin-with-all-ancestors policy to prevent cross-origin iframe abuse.
*   `ValidateAppIdExtension(...)`: Handles the validation for the legacy U2F AppID extension.
*   `RemoteValidation::Create(...)`: Initiates an out-of-band HTTPS `GET` request to a `.well-known` URL to validate certain RP IDs, adding another layer of security checks.

## 5. Related Files

*   `content/browser/webauth/authenticator_impl.cc`: The main implementation of the `blink.mojom.Authenticator` interface. It creates and uses a `WebAuthRequestSecurityChecker` instance to validate every incoming request.
*   `content/public/browser/content_browser_client.h`: The interface that allows the embedder (e.g., Chrome) to inject its own policies, such as the allowlist for the remote desktop override.
*   `content/browser/renderer_host/render_frame_host_impl.h`: The `RenderFrameHost` is used by the checker to access the frame tree and validate ancestor origins.