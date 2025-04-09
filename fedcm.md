# Component: Federated Credential Management (FedCM) API / Web Identity

## 1. Component Focus
*   **Functionality:** Implements the Federated Credential Management API ([Explainer](https://github.com/fedidcg/FedCM/blob/main/explainer.md)), enabling browser-mediated federated identity flows that aim to improve privacy over traditional redirect-based methods. Part of the broader Web Identity (WebID) effort.
*   **Key Logic:** Handles communication between Relying Party (RP), Identity Provider (IdP), fetching manifests (`/.well-known/web-identity`), displaying account choosers (modal or bubble UI), issuing identity assertions, managing login status. Includes handling identity protocols, storing/managing identity data.
*   **Core Files:**
    *   `content/browser/webid/`: Core browser-side logic for WebID/FedCM (e.g., `federated_auth_request_impl.cc`, `federated_provider.cc`, `webid_utils.cc`).
    *   `chrome/browser/webid/`: Chrome-specific UI and integration.
    *   `chrome/browser/ui/views/webid/`: Desktop UI views (e.g., `fedcm_account_selection_view_desktop.cc`).
    *   `third_party/blink/renderer/modules/credentialmanagement/`: Renderer-side APIs (e.g., `CredentialsContainer`, `FederatedCredential`).
    *   `components/webid/`: Potentially shared components.

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring/Spoofing:** The FedCM UI (especially the bubble dialog) can obscure or be obscured by other UI elements, leading to user confusion or hidden interactions.
    *   **VRP Pattern (FedCM Obscures Autofill):** FedCM bubble dialog rendering over Autofill prompts, potentially enabling hidden autofill selection. The bubble **does not steal focus upon showing** (similar to the effect of `ShowInactive`), which allows the Autofill prompt underneath to remain active (VRP: `339481295`, `340893685`; VRP2.txt#7963 context notes this behavior). See [autofill.md](autofill.md).
    *   **VRP Pattern (PiP Obscures FedCM):** Video/Document Picture-in-Picture windows obscuring the FedCM prompt bubble, potentially allowing hidden login attempts. (VRP: `339654392`; VRP2.txt#12993). See [picture_in_picture.md](picture_in_picture.md).
    *   **VRP Pattern (Input Protections):** The lack of robust input protections (e.g., interaction delays similar to Autofill/Permissions) when the FedCM bubble is obscured allowed hidden logins via keyboard interaction (Related to VRP: `339654392`).
*   **Incorrect UI Placement:** FedCM UI rendering outside the bounds of its initiating window, especially in non-maximized or small windows.
    *   **VRP Pattern (Bubble Outside Window):** FedCM bubble rendering far below or partially offscreen when initiated from small/positioned windows, potentially leading to origin confusion or hidden interactions. (VRP: `338233148`; VRP2.txt#13024).
*   **Origin Display Issues:** Failure to correctly display the requesting origin in the FedCM UI, particularly with opaque origins.
    *   **VRP Pattern (Opaque Origin Display):** FedCM prompts showing `://` instead of a meaningful origin when initiated from an opaque origin (e.g., sandboxed iframe, data URL). (VRP: `340893685`; VRP2.txt#13066).
*   **Policy Bypass (e.g., SameSite):** Identity flows potentially being used to bypass cookie policies or other origin-based restrictions.
*   **Information Leaks:** Leaking information about user accounts, IdPs, or relationships between sites.
*   **Authentication/Authorization Flaws:** Weaknesses in the authentication protocols used or how authorization decisions are made within the FedCM flow.

## 3. Further Analysis and Potential Issues
*   **Dialog Types:** Analyze differences in security posture between the bubble (`AccountSelectionBubbleView`) and modal (`FedCmModalDialogView`) FedCM dialogs regarding obscuring, placement, and focus handling.
*   **Focus Management:** How does focus shift when FedCM UI appears and disappears? Can focus manipulation be used in attacks? The lack of focus stealing by the bubble (similar effect to `ShowInactive` - see VRP2.txt#7963 context) was key in allowing Autofill obscuring. Investigate the widget creation and showing logic in base classes like `BubbleDialogDelegateView`.
*   **Origin Handling:** Deep dive into `webid::FormatUrlForDisplay` and its callers (`FederatedAuthRequestImpl::GetTopFrameOriginForDisplay`, `GetEmbeddingOrigin`) to ensure opaque origins are handled securely in all UI states (accounts dialog, error dialogs, loading dialogs). (VRP: `340893685`).
*   **Window Interaction:** How does FedCM behave when initiated from different window types (popups, PWAs, potentially extension contexts)? Does the UI placement logic (`AccountSelectionBubbleView::GetBubbleBounds`) handle all scenarios correctly? (VRP: `338233148`).
*   **Input Protection:** Does the FedCM UI implement sufficient input protection (like `InputEventActivationProtector` or delays) to prevent clickjacking/tapjacking/keyjacking, especially when potentially obscured? (VRP: `339654392` implies this was lacking).
*   **Race Conditions:** Are there race conditions between showing/hiding the FedCM UI and other browser UI updates or navigation events?
*   **Protocol Handling:** Analyze how specific identity protocols (e.g., OpenID Connect variants potentially used by FedCM IdPs) are handled within the FedCM flow. Are there parsing or state management vulnerabilities?
*   **Storage Security:** How is identity-related state (e.g., login status, account information) stored specifically for FedCM? Is it adequately protected and isolated?
*   **Error Handling:** Ensure error messages within the FedCM flow don't leak sensitive information.

## 4. Code Analysis
*   `FederatedAuthRequestImpl`: Manages the overall FedCM request flow, permission checks, calls to UI via `RequestDialogController`. Methods like `ShowAccountsDialog`, `ShowFailureDialog`, `FormatOriginForDisplay`, `GetEmbeddingOrigin`.
*   `IdentityDialogController` (`chrome/browser/ui/webid/`): Implements `RequestDialogController`, creates and manages platform-specific views.
*   `FederatedProvider`: Handles communication with IdPs for FedCM.
*   `AccountSelectionBubbleView` / `FedCmModalDialogView` (`chrome/browser/ui/views/webid/`): Desktop UI implementations. Inherit from `BubbleDialogDelegateView`.
    *   `GetBubbleBounds()` (in BubbleView): Overrides default bubble placement, potential source of positioning issues (VRP: `338233148`).
    *   Check base class (`BubbleDialogDelegateView`) for widget creation (`CreateBubble`) and showing logic (`Show`/`ShowInactive`).
    *   Event Handling: How user clicks/interactions on accounts are processed. Need input protection checks.
*   `webid::FormatUrlForDisplay`: Utility for formatting origins. Needs robust opaque origin handling (VRP: `340893685`).
*   `CredentialsContainer` (Blink): Renderer-side API for credential management, including FedCM.
*   Interaction with PiP: Check how FedCM UI interacts with `PictureInPictureWindowManager` or occlusion trackers (`PictureInPictureOcclusionTracker`) (Related to VRP: `339654392`).

## 5. Areas Requiring Further Investigation
*   **Modal Dialog Security:** Specifically test the FedCM modal dialog for similar obscuring, placement, and input protection issues as found with the bubble dialog.
*   **Opaque Origin Handling:** Confirm `webid::FormatUrlForDisplay` and its callers correctly handle opaque origins in *all* FedCM UI states (including errors, loading, multi-IdP scenarios).
*   **Input Protection:** Audit FedCM UI views for robust input protection mechanisms (delays, visibility checks) to prevent interaction when obscured or immediately upon display.
*   **UI Placement Logic:** Review `AccountSelectionBubbleView::GetBubbleBounds` logic to ensure it handles various window sizes/positions without rendering offscreen or far from the anchor.
*   **Cross-Component Interactions:** Test interactions with Autofill, Permissions, PiP, Extensions thoroughly.
*   **Security of non-FedCM identity features:** Are there other relevant APIs or mechanisms under the WebID umbrella (outside the direct FedCM flow) that need investigation?
*   **Interaction with other identity/login mechanisms:** How does FedCM interact with password manager, WebAuthn, etc.?
*   **Privacy aspects:** Analyze potential tracking vectors or information leaks related to IdP/RP communication mediated by the browser within the FedCM context.

## 6. Related VRP Reports
*   VRP: `339481295` ($3000): Autofill prompt can be obscured by FedCM bubble dialog (Context: VRP2.txt#7963)
*   VRP: `340893685` ($2000): FedCM prompts do not show origin if initiator origin is opaque (Context: VRP2.txt#13066)
*   VRP: `339654392` ($2000): FedCM prompt bubble can be obscured by Video/Document PiP window, allow for hidden login (Context: VRP2.txt#12993)
*   VRP: `338233148` ($2000): FedCM prompt bubble renders outside of opening window, causing various issues (Context: VRP2.txt#13024)

*(See also [autofill.md](autofill.md), [picture_in_picture.md](picture_in_picture.md), [privacy.md](privacy.md))*