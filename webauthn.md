# Web Authentication (WebAuthn)

## 1. Component Focus
*   **Functionality:** Implements the W3C Web Authentication API ([Spec](https://www.w3.org/TR/webauthn-2/)), enabling strong, public-key based authentication using authenticators (FIDO security keys, platform authenticators like Touch ID/Windows Hello). Handles credential registration (`navigator.credentials.create()`) and authentication (`navigator.credentials.get()`).
*   **Key Logic:** Processing API calls from Blink, validating options (Relying Party ID, challenge, user verification requirements), interacting with the FIDO device layer (`device/fido`), managing platform authenticators, handling user consent/interaction (though UI is often platform-specific), ensuring secure context requirements, enforcing origin checks.
*   **Core Files:**
    *   `content/browser/webauth/authenticator_impl.cc`/`.h`: Implements `blink::mojom::Authenticator` Mojo interface used by the renderer. Entry point for browser-side logic. Delegates core operations.
    *   `content/browser/webauth/authenticator_common_impl.cc`/`.h`: Common logic shared between `AuthenticatorImpl` and potentially other callers. Handles parsing options, permissions checks, and orchestrates calls to the device layer.
    *   `device/fido/`: Core FIDO protocol implementation, authenticator discovery (CTAP2, U2F), and platform API abstraction. **Key sub-component handling actual authenticator interaction.** (e.g., `fido_authenticator.h`, `fido_discovery_base.h`, platform-specific directories like `mac/`, `win/`).
    *   `third_party/blink/renderer/modules/credentialmanagement/`: Renderer-side API implementation (`CredentialsContainer`, `PublicKeyCredential`).
    *   `components/device_event_log/`: Used for FIDO logging.

## 2. Potential Logic Flaws & VRP Relevance

WebAuthn is security-critical, and flaws can compromise user accounts.

*   **Incorrect Origin / Relying Party ID (RP ID) Handling:** Failing to correctly validate the RP ID against the requesting origin (`AuthenticatorImpl::origin()`) could allow impersonation. Need strict validation according to spec rules (domain matching).
*   **Scheme Handling (`fido://`) Bypass/Hijack:** The internal `fido://` scheme is used for communication.
    *   **VRP Pattern (`fido://` Hijack):** A past vulnerability allowed hijacking of the `fido://` scheme (VRP2.txt#3016). This suggests potential flaws in how these internal URLs are generated, parsed, or protected from being initiated or manipulated by web content, perhaps bypassing security checks in navigation or subresource request pathways. Requires careful audit of any code path handling `fido:` URLs.
*   **Bypassing User Verification (UV) / User Presence (UP):** Flaws in logic checking `uv` or `up` flags, or in interacting with platform authenticators that perform these checks (`device/fido/` layer).
*   **Insecure Credential Storage/Management:** Vulnerabilities in platform-specific backends (`device/fido/mac/`, `win/`, etc.) or key stores where credentials reside.
*   **Replay Attacks:** Insufficient validation of challenges or signatures allowing captured assertions to be replayed. Relies on robust cryptographic checks within `AuthenticatorCommonImpl` and the `device/fido` layer.
*   **Incorrect Client Data Handling:** Improper validation or handling of `clientDataJSON` or its hash (`collectedClientData.hash`).
*   **Information Leaks:** Leaking information about available authenticators or user credentials beyond spec allowances (e.g., via `IsUserVerifyingPlatformAuthenticatorAvailable`).
*   **Platform Authenticator Interaction Bugs:** Vulnerabilities specific to the implementation of platform authenticators (Touch ID, Windows Hello) within `device/fido/`.

## 3. Further Analysis and Potential Issues
*   **Origin/RP ID Validation:** Deep dive into `AuthenticatorCommonImpl::MakeCredential` and `GetAssertion`. How is the RP ID extracted from options and validated against the `RenderFrameHost`'s origin (`AuthenticatorImpl::origin()`)? Are all spec rules (e.g., registrable domain suffix matching) correctly implemented?
*   **`fido://` Scheme Security:** Where is the `fido://` scheme registered or handled outside the `device/fido` layer? Can web content trigger navigations or requests to `fido://` URLs? Are there any components (e.g., protocol handlers, navigation throttles) that might incorrectly process or allow access to these URLs? (Related VRP2.txt#3016).
*   **Device Layer Interaction (`device/fido/`):** How does `AuthenticatorCommonImpl` discover and communicate with authenticators via `device/fido`? Analyze the interfaces (`FidoDiscoveryFactory`, `FidoAuthenticator`) and data passed between `content/browser/webauth` and `device/fido`. Are there opportunities for state confusion or insecure data transfer?
*   **User Verification Logic:** Trace the handling of the `uv` option. How is platform user verification triggered and its result securely communicated back from `device/fido`?
*   **Challenge Binding:** Verify that challenges provided by the RP are correctly incorporated into signatures and validated during `GetAssertion`.
*   **Extension Handling:** How does WebAuthn handle requests originating from extensions? Are extension permissions checked? Are RP IDs validated correctly against extension origins?
*   **Conditional UI (`IsConditionalMediationAvailable`):** Analyze the security implications of the conditional UI flow.
*   **Secure Context Enforcement:** Where is the secure context requirement enforced for WebAuthn API calls?

## 4. Code Analysis
*   `AuthenticatorImpl`: Implements Mojo interface. Primarily validates frame origin (`origin()`) and delegates to `AuthenticatorCommonImpl`.
*   `AuthenticatorCommonImpl`: Core logic. Parses `PublicKeyCredentialCreationOptions` / `PublicKeyCredentialRequestOptions`. Interacts with `device::FidoDiscoveryFactory` and `device::FidoAuthenticator`. **Key area for RP ID validation, challenge handling, UV/UP flag processing.**
*   `device/fido/`: Handles authenticator discovery, CTAP2/U2F protocols, platform API interaction (macOS Touch ID, Windows Hello, etc.). **Complex logic, potential platform-specific bugs.** Contains FIDO constants (like `FidoConstants::kFidoScheme` although its direct usage seems limited based on search).
*   `PublicKeyCredential*Options`: Blink structures representing options passed from the renderer. Need secure parsing/validation in `AuthenticatorCommonImpl`.
*   `CollectedClientData`: Contains critical data like challenge and origin, hashed for signing. Needs correct construction and validation.

## 5. Areas Requiring Further Investigation
*   **RP ID vs Origin Validation:** Audit the exact logic in `AuthenticatorCommonImpl` where RP ID is compared against the frame origin. Check edge cases (subdomains, ports, Punycode).
*   **`fido://` Scheme Handling:** **Search the entire codebase (not just `content/browser/`) for registration and handling of the `fido:` scheme.** Are navigations to `fido:` possible and appropriately blocked? Are there URL parsers or handlers that might misinterpret `fido:` URLs? (Relates to VRP2.txt#3016).
*   **`device/fido` Security:** Perform in-depth review or fuzzing of the `device/fido` layer, especially platform-specific backends and CTAP2 command handling.
*   **State Management:** Analyze state management within `AuthenticatorCommonImpl` and `device/fido` during complex flows (e.g., multiple authenticators, cancellation, errors).
*   **Credential Storage Security:** Investigate platform-specific credential storage mechanisms accessed via `device/fido`.

## 6. Related VRP Reports
*   VRP2.txt#3016 (`fido://` Hijack) - Indicates potential flaws in scheme handling/isolation.

## 7. Cross-References
*   [navigation.md](navigation.md) (Potentially related to scheme handling)
*   [ipc.md](ipc.md) (Mojo communication)
