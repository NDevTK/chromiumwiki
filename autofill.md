# Component: Autofill

## 1. Component Focus
*   **Functionality:** Provides automatic filling of forms (addresses, credit cards, passwords) based on previously saved user data. Includes UI elements like dropdown suggestions (Autofill popup) and explicit prompts (e.g., card unmasking, save prompts).
*   **Key Logic:** Detecting fillable forms, querying saved data (`AutofillTable`, `PasswordFormStore`), ranking suggestions, displaying UI (`AutofillPopupControllerImpl`, various Views), handling user interaction, filling data securely, managing data saving/updating. Cross-origin iframe interactions are complex. Password Autofill interacts closely with the Password Manager.
*   **Core Files:**
    *   `components/autofill/core/browser/`: Core logic (e.g., `autofill_manager.cc`, `form_structure.cc`, data models).
    *   `components/autofill/core/common/`: Shared types and constants.
    *   `chrome/browser/ui/autofill/`: UI controllers and logic (e.g., `autofill_popup_controller_impl.cc`).
    *   `chrome/browser/ui/views/autofill/`: Desktop UI views (e.g., `popup/autofill_popup_view.cc`, `payments/virtual_card_enroll_bubble_views.cc`).
    *   `components/password_manager/core/browser/`: Password Autofill logic. ([password_manager.md](password_manager.md))
    *   `third_party/blink/renderer/core/exported/`, `third_party/blink/renderer/modules/autofill/`: Renderer-side logic.

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring / Spoofing / Clickjacking:** Autofill popups or prompts obscuring or being obscured by other UI, leading to unintentional data filling/saving or bypassing user confirmation.
    *   **VRP Pattern (FedCM/Permission Obscures Autofill):** FedCM bubble dialog (VRP: `339481295`, VRP2.txt#7963) or Permission prompts (PEPC) (VRP: `342194497`, VRP2.txt#6928) rendering over Autofill popups/prompts. Because these obscuring elements often don't steal focus (using `ShowInactive` or similar), the underlying Autofill UI remains interactable via keyboard, potentially leading to accidental data selection or submission without user awareness. Autofill UI itself **appears to lack** robust input protection mechanisms like `InputEventActivationProtector` (used by Permissions) to mitigate this specific type of attack. See [fedcm.md](fedcm.md), [permissions.md](permissions.md).
    *   **VRP Pattern (Autofill Obscures Other UI):** Autofill popups obscuring critical security UI like permission prompts.
    *   **VRP Pattern (Popup Placement/Focus):** Incorrect placement or focus handling of Autofill popups, especially relative to iframes or other browser windows, enabling spoofing or interaction issues. Clicks intended for one frame targeting Autofill in another (VRP: `1474196`).
*   **Cross-Origin Data Leakage:** Information from one origin's Autofill data being accessible to another origin, often involving iframes.
    *   **VRP Pattern (iframe Boundaries):** Bugs in how Autofill data is associated with frames or how suggestions are filtered based on origin, potentially allowing a cross-origin iframe to trigger or access Autofill suggestions meant for the parent or another frame. Incorrect `SubmittedOrigin` checks (VRP: `1301164`). Iframe focus issues leading to data leakage (VRP: `1474196`).
    *   **VRP Pattern (Password Manager Interaction):** Leaking password existence or username hints across origins via Autofill previews or interactions (VRP: `1487110`). See [password_manager.md](password_manager.md).
*   **Incorrect Data Filling/Saving:** Autofill data being filled into the wrong fields, or data being saved under the wrong origin/profile.
*   **Bypassing Security Prompts:** Circumventing prompts required for sensitive actions (e.g., CVC unmasking, virtual card enrollment).

## 3. Further Analysis and Potential Issues
*   **Input Protection:** **Crucially, investigate adding input protection (e.g., `InputEventActivationProtector`) to Autofill popups and prompts** to mitigate keyjacking/clickjacking when obscured by non-focus-stealing UI like FedCM or permission prompts (Related to VRP: `339481295`, `342194497`).
*   **Occlusion Detection:** How does Autofill detect if its UI is obscured by other browser UI (PiP, FedCM, Permissions)? Is the detection robust? Does it correctly hide or disable interaction when obscured? (Related to VRP: `339481295`, `342194497`).
*   **iframe Origin Checks:** Deep dive into cross-origin iframe scenarios. How are `FormStructure::host_frame()`, `AutofillManager::ResolveRawFormData()`, and origin checks in `AutofillPopupControllerImpl` used to prevent cross-origin leaks or triggering? Are there edge cases (e.g., redirects, dynamic iframe manipulation) that bypass these? (Related to VRP: `1301164`, `1474196`).
*   **Focus Tracking:** Analyze how Autofill tracks focus changes between frames and browser windows. Can focus be manipulated to cause Autofill suggestions for one frame to appear or be interacted with while another frame has focus? (Related to VRP: `1474196`).
*   **Password Manager Interaction:** Review how Password Autofill previews usernames or indicates saved passwords. Ensure this doesn't leak information across origins, especially considering potential timing attacks or UI state manipulation (Related to VRP: `1487110`).
*   **Data Storage Security:** How is Autofill data stored and encrypted? Are there risks related to profile switching or data access by other browser components?

## 4. Code Analysis
*   `AutofillManager`: Core browser-side class managing Autofill logic per `RenderFrameHost`. Handles form parsing, data querying, filling.
*   `AutofillPopupControllerImpl`: Manages the Autofill popup UI, deciding what to show and handling selection. Origin filtering logic is critical here.
*   `AutofillPopupView`: Desktop UI view for the Autofill suggestion popup. **Appears to lack `InputEventActivationProtector`.**
*   Other Autofill Views (e.g., `SaveCardBubbleViews`, `VirtualCardEnrollBubbleViews`): Need checks for input protection and obscuring issues.
*   `FormStructure`: Represents a parsed form, including field types and origin information (`host_frame()`, `source_url()`). Validity of this data across frames is key.
*   `AutofillTable`: Database interaction for storing/retrieving Autofill profiles.
*   Password Manager components (`PasswordManagerClient`, `PasswordFormManager`): Interaction points for password Autofill.

## 5. Areas Requiring Further Investigation
*   **Add Input Protection:** **Prioritize investigating the feasibility and impact of adding `InputEventActivationProtector` or similar mechanisms to Autofill UI elements.**
*   **iframe Scenarios:** Design and test complex iframe scenarios (cross-origin, nested, sandboxed, dynamically added/removed) to probe origin checks and focus handling.
*   **Obscuring Interactions:** Test interactions where Autofill UI is obscured by various other browser elements (PiP, Permissions, FedCM, Extensions) and vice-versa, focusing on keyboard and click interactions.
*   **Password Field Previews:** Analyze timing and conditions under which password Autofill might reveal information pre-fill.

## 6. Related VRP Reports
*   VRP: `339481295` ($3000): Autofill prompt can be obscured by FedCM bubble dialog (Context: VRP2.txt#7963)
*   VRP: `342194497` ($3000): Autofill prompt can be obscured by permission prompt (PEPC) (Context: VRP2.txt#6928)
*   VRP: `1474196`: Address form autofill triggers on wrong frame due to focus issue.
*   VRP: `1301164`: Autofill data leakage via cross-origin iframe.
*   VRP: `1487110`: Password manager hint leakage.

*(See also [fedcm.md](fedcm.md), [permissions.md](permissions.md), [password_manager.md](password_manager.md), [privacy.md](privacy.md))*
