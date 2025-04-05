# Component: Blink > Identity > FedCM

## 1. Component Focus
*   Focuses on the Federated Credential Management API implementation.
*   Handles identity provider (IdP) communication, user account selection, and credential issuance.
*   Involves UI elements like the account chooser bubble/dialog.
*   Relevant files might include:
    *   `content/browser/webid/federated_auth_request_impl.cc`
    *   `chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.cc`
    *   `third_party/blink/renderer/modules/credentialmanagement/credentials_container.cc`

## 2. Potential Logic Flaws & VRP Relevance
*   UI Obscuring: FedCM UI (bubble/dialog) obscuring other sensitive UI like Autofill prompts (VRP: 339481295, 340893685), or being obscured by other UI like PiP windows (VRP: 339654392).
*   Origin Display Issues: Failure to show the correct origin, especially with opaque initiator origins (VRP: 340893685).
*   Incorrect UI Placement: Dialog rendering outside the initiator window (VRP: 338233148).
*   Lack of Input Protections causing hidden login when obscured (VRP: 339654392).

## 3. Further Analysis and Potential Issues
*   *(Detailed analysis of dialog types (bubble vs. modal), focus handling, UI interaction logic, and origin handling to be added based on code review and VRP details.)*
*   How does FedCM interact with different window types (popups, PWA windows)?
*   Are there race conditions related to showing/hiding the FedCM UI and other browser UI?

## 4. Code Analysis
*   *(Specific code snippets related to dialog creation (`AccountSelectionBubbleView`), UI positioning (`GetBubbleBounds`), origin formatting (`FormatUrlForDisplay`), and focus management (`ShowInactive`) to be added.)*

## 5. Areas Requiring Further Investigation
*   Interaction between FedCM modal dialog and other UI elements (Autofill, PiP).
*   Complete analysis of origin handling, especially for opaque origins across different FedCM UI states (error dialogs, loading dialogs).
*   Potential input protection vulnerabilities when obscured.

## 6. Related VRP Reports
*   VRP #339481295 (P1, $3000): Security: Autofill prompt can be obscured by FedCM bubble dialog
*   VRP #340893685 (P1, $2000): Security: FedCM prompts do not show origin if initiator origin is opaque
*   VRP #339654392 (P1, $2000): Security: FedCM prompt bubble can be obscured by Video/Document PiP window, allow for hidden login
*   VRP #338233148 (P2, $2000): Security: FedCM prompt bubble renders outside of opening window, causing various issues

*(This list should be reviewed against VRP.txt/VRP2.txt for completeness regarding FedCM reports).*