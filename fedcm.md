# Component: Federated Credential Management (FedCM) API / Web Identity

## 1. Component Focus
*   **Functionality:** Implements the Federated Credential Management API ([Explainer](https://github.com/fedidcg/FedCM/blob/main/explainer.md)), enabling browser-mediated federated identity flows that aim to improve privacy over traditional redirect-based methods. Part of the broader Web Identity (WebID) effort.
*   **Key Logic:** Handles communication between Relying Party (RP), Identity Provider (IdP), fetching manifests (`/.well-known/web-identity`), displaying account choosers (modal or bubble UI), issuing identity assertions, managing login status. Includes handling identity protocols, storing/managing identity data.
*   **Core Files:**
    *   `content/browser/webid/`: Core browser-side logic for WebID/FedCM (e.g., `federated_auth_request_impl.cc`, `federated_provider.cc`, `webid_utils.cc`).
    *   `chrome/browser/webid/`: Chrome-specific UI and integration.
    *   `chrome/browser/ui/views/webid/`: Desktop UI views (e.g., `fedcm_account_selection_view_desktop.h/.cc`, `account_selection_bubble_view.h/.cc`).
    *   `chrome/browser/ui/webid/`: UI controllers (`identity_dialog_controller.cc`).
    *   `ui/views/input_event_activation_protector.h/.cc`: Implements time-based input protection.
    *   `third_party/blink/renderer/modules/credentialmanagement/`: Renderer-side APIs (e.g., `CredentialsContainer`, `FederatedCredential`).
    *   `components/webid/`: Potentially shared components.

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring / Spoofing:** The FedCM UI (especially the bubble dialog) can obscure or be obscured by other UI elements, leading to user confusion or hidden interactions.
    *   **VRP Pattern (FedCM Obscures Other UI - e.g., Autofill/Permissions):** FedCM bubble dialog rendering over Autofill or Permission prompts (VRP: `339481295`, VRP2.txt#7963; VRP2.txt#6928 context). `FedCmAccountSelectionViewDesktop` uses `Show()` (not `ShowInactive()`) but allows the underlying tab to receive mouse events (`ScopedAcceptMouseEventsWhileWindowInactive`). More importantly, the FedCM bubble *uses* `InputEventActivationProtector` to protect itself against immediate interaction. However, if the underlying UI (like a Permission prompt shown inactive) **lacks** its own input protection, it can still be activated via keyboard while obscured by the FedCM bubble. See [autofill.md](autofill.md), [permissions.md](permissions.md).
    *   **VRP Pattern (PiP Obscures FedCM):** Video/Document Picture-in-Picture windows obscuring the FedCM prompt bubble. `FedCmAccountSelectionViewDesktop` *does* track PiP occlusion and uses `InputEventActivationProtector`, offering some defense, but bypasses were still found (VRP: `339654392`; VRP2.txt#12993), suggesting the protection might have limitations or specific bypasses. See [picture_in_picture.md](picture_in_picture.md).
*   **Incorrect UI Placement:** FedCM UI rendering outside the bounds of its initiating window, especially in non-maximized or small windows.
    *   **VRP Pattern (Bubble Outside Window):** FedCM bubble rendering far below or partially offscreen when initiated from small/positioned windows, potentially leading to origin confusion or hidden interactions. (VRP: `338233148`; VRP2.txt#13024). Likely related to `AccountSelectionBubbleView::GetBubbleBounds`.
*   **Origin Display Issues:** Failure to correctly display the requesting origin in the FedCM UI, particularly with opaque origins.
    *   **VRP Pattern (Opaque Origin Display):** FedCM prompts showing `://` instead of a meaningful origin when initiated from an opaque origin (e.g., sandboxed iframe, data URL). (VRP: `340893685`; VRP2.txt#13066). Relates to `webid::FormatUrlForDisplay`.
*   **Policy Bypass (e.g., SameSite):** Identity flows potentially being used to bypass cookie policies or other origin-based restrictions.
*   **Information Leaks:** Leaking information about user accounts, IdPs, or relationships between sites.
*   **Authentication/Authorization Flaws:** Weaknesses in the authentication protocols used or how authorization decisions are made within the FedCM flow.

## 3. Further Analysis and Potential Issues
*   **Input Protection Effectiveness:** How robust is `InputEventActivationProtector` as used in `FedCmAccountSelectionViewDesktop`? Does it cover all interaction types (click, key, tap)? Are there timing windows or specific event sequences that bypass it, especially in occlusion scenarios (VRP: `339654392`)?
*   **Modal Dialog Security:** Analyze the `FedCmModalDialogView` for similar obscuring, placement, and input protection issues as the bubble dialog.
*   **Focus Management:** How does focus shift when FedCM UI appears and disappears? Can focus manipulation be used in attacks? Analyze the interaction between `Show()`, `ScopedAcceptMouseEventsWhileWindowInactive`, and underlying UI focus state, especially when interacting with UI like Permissions prompts that use `ShowInactive()`.
*   **Origin Handling:** Deep dive into `webid::FormatUrlForDisplay` and its callers (`FederatedAuthRequestImpl::GetTopFrameOriginForDisplay`, `GetEmbeddingOrigin`) to ensure opaque origins are handled securely in all UI states (accounts dialog, error dialogs, loading dialogs). (VRP: `340893685`).
*   **Window Interaction:** How does FedCM behave when initiated from different window types (popups, PWAs, potentially extension contexts)? Does the UI placement logic (`AccountSelectionBubbleView::GetBubbleBounds`) handle all scenarios correctly? (VRP: `338233148`).
*   **Race Conditions:** Are there race conditions between showing/hiding the FedCM UI and other browser UI updates or navigation events?
*   **Protocol Handling:** Analyze how specific identity protocols (e.g., OpenID Connect variants potentially used by FedCM IdPs) are handled within the FedCM flow. Are there parsing or state management vulnerabilities?
*   **Storage Security:** How is identity-related state (e.g., login status, account information) stored specifically for FedCM? Is it adequately protected and isolated?
*   **Error Handling:** Ensure error messages within the FedCM flow don't leak sensitive information.

## 4. Code Analysis
*   `FederatedAuthRequestImpl`: Manages the overall FedCM request flow, permission checks, calls to UI via `RequestDialogController`. Methods like `ShowAccountsDialog`, `ShowFailureDialog`, `FormatOriginForDisplay`, `GetEmbeddingOrigin`.
*   `IdentityDialogController` (`chrome/browser/ui/webid/`): Implements `RequestDialogController`, creates and manages platform-specific views (`FedCmAccountSelectionView`).
*   `FederatedProvider`: Handles communication with IdPs for FedCM.
*   `FedCmAccountSelectionViewDesktop` (`chrome/browser/ui/views/webid/`): Desktop UI controller. Inherits `AccountSelectionView`, `PictureInPictureOcclusionObserver`. Manages `dialog_widget_`. Uses `InputEventActivationProtector`. Calls `Show()` on widget, not `ShowInactive()`. Tracks PiP occlusion. Uses `ScopedAcceptMouseEventsWhileWindowInactive` for bubble dialog.
*   `AccountSelectionBubbleView` / `FedCmModalDialogView` (`chrome/browser/ui/views/webid/`): Specific desktop UI implementations. Inherit from `AccountSelectionViewBase` / `BubbleDialogDelegateView` or `DialogDelegate`.
    *   `GetBubbleBounds()` (in BubbleView): Overrides default bubble placement, potential source of positioning issues (VRP: `338233148`).
*   `webid::FormatUrlForDisplay`: Utility for formatting origins. Needs robust opaque origin handling (VRP: `340893685`).
*   `InputEventActivationProtector` (`ui/views/input_event_activation_protector.cc`): Provides time-based input protection. Used by `FedCmAccountSelectionViewDesktop`.
*   `CredentialsContainer` (Blink): Renderer-side API for credential management, including FedCM.
*   Interaction with PiP: Uses `PictureInPictureOcclusionTracker` via `ScopedPictureInPictureOcclusionObservation`. VRP `339654392` suggests bypass possible.

## 5. Areas Requiring Further Investigation
*   **Input Protection Analysis:** Deep dive into `InputEventActivationProtector` implementation and usage in FedCM. Compare effectiveness against VRP bypass techniques.
*   **Occlusion Scenarios:** Test FedCM bubble display alongside Permission prompts (shown inactive) and Autofill prompts to replicate VRP conditions. Analyze focus state and event propagation to understand how interaction bypasses occur despite FedCM's own input protector.
*   **PiP Occlusion Bypass:** Investigate why PiP occlusion checks might fail (VRP: `339654392`). Is it related to timing, widget properties, or tracker limitations?
*   **Opaque Origin Handling:** Confirm `webid::FormatUrlForDisplay` and its callers correctly handle opaque origins in *all* FedCM UI states (including errors, loading, multi-IdP scenarios).
*   **UI Placement Logic:** Review `AccountSelectionBubbleView::GetBubbleBounds` logic to ensure it handles various window sizes/positions without rendering offscreen or far from the anchor.
*   **Cross-Component Interactions:** Test interactions with Autofill, Permissions, PiP, Extensions thoroughly.
*   **Security of non-FedCM identity features:** Are there other relevant APIs or mechanisms under the WebID umbrella?
*   **Interaction with other identity/login mechanisms:** How does FedCM interact with password manager, WebAuthn, etc.?
*   **Privacy aspects:** Analyze potential tracking vectors or information leaks related to IdP/RP communication mediated by the browser within the FedCM context.

## 6. Related VRP Reports
*   VRP: `339481295` ($3000): Autofill prompt can be obscured by FedCM bubble dialog (Context: VRP2.txt#7963)
*   VRP: `340893685` ($2000): FedCM prompts do not show origin if initiator origin is opaque (Context: VRP2.txt#13066)
*   VRP: `339654392` ($2000): FedCM prompt bubble can be obscured by Video/Document PiP window, allow for hidden login (Context: VRP2.txt#12993)
*   VRP: `338233148` ($2000): FedCM prompt bubble renders outside of opening window, causing various issues (Context: VRP2.txt#13024)
*   VRP2.txt#6928: Permission prompt obscured by other permission prompt (Similar interaction pattern).

*(See also [autofill.md](autofill.md), [permissions.md](permissions.md), [picture_in_picture.md](picture_in_picture.md), [privacy.md](privacy.md))*