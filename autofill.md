# Component: Autofill

## 1. Component Focus
*   **Functionality:** Provides automatic filling of forms (addresses, credit cards, passwords) based on previously saved user data. Includes UI elements like dropdown suggestions (Autofill popup) and explicit prompts (e.g., card unmasking, save prompts).
*   **Key Logic:** Detecting fillable forms (renderer), sending form data to browser, parsing forms (`FormStructure`), querying saved data (`AutofillTable`, `PasswordFormStore`), ranking suggestions, displaying UI (`AutofillPopupControllerImpl`, various Views), handling user interaction, sending fill actions back to renderer, managing data saving/updating. Cross-origin iframe interactions are complex. Password Autofill interacts closely with the Password Manager.
*   **Core Files:**
    *   `components/autofill/content/renderer/autofill_agent.cc`: Renderer-side logic, interacts with Blink forms, sends IPC to browser.
    *   `components/autofill/content/browser/content_autofill_driver.cc`: Browser-side driver per frame, receives IPC, "lifts" data, interacts with manager.
    *   `components/autofill/core/browser/foundations/browser_autofill_manager.cc`: Core browser-side logic per frame, manages `FormStructure` cache, generates suggestions, triggers filling.
    *   `components/autofill/core/browser/`: Common core logic (e.g., `form_structure.cc`, data models).
    *   `components/autofill/core/common/`: Shared types and constants.
    *   `chrome/browser/ui/autofill/`: UI controllers and logic (e.g., `autofill_popup_controller_impl.cc`, `autofill_external_delegate.h`).
    *   `chrome/browser/ui/views/autofill/`: Desktop UI views (e.g., `popup/autofill_popup_view.cc`, `payments/virtual_card_enroll_bubble_views.cc`).
    *   `components/password_manager/core/browser/`: Password Autofill logic. ([password_manager.md](password_manager.md))
    *   `third_party/blink/public/web/web_autofill_client.h`: Blink interface implemented by `AutofillAgent`.

## 2. Potential Logic Flaws & VRP Relevance
*   **UI Obscuring / Spoofing / Clickjacking:** Autofill popups or prompts obscuring or being obscured by other UI, leading to unintentional data filling/saving or bypassing user confirmation.
    *   **VRP Pattern (FedCM/Permission Obscures Autofill):** FedCM bubble dialog (VRP: `339481295`, VRP2.txt#7963) or Permission prompts (PEPC) (VRP: `342194497`, VRP2.txt#6928) rendering over Autofill popups/prompts. Because these obscuring elements often don't steal focus (using `ShowInactive` or similar), the underlying Autofill UI remains interactable via keyboard, potentially leading to accidental data selection or submission without user awareness. Autofill UI itself **appears to lack** robust input protection mechanisms like `InputEventActivationProtector` (used by Permissions) to mitigate this specific type of attack. See [fedcm.md](fedcm.md), [permissions.md](permissions.md).
    *   **VRP Pattern (Autofill Obscures Other UI):** Autofill popups obscuring critical security UI like permission prompts.
    *   **VRP Pattern (Popup Placement/Focus):** Incorrect placement or focus handling of Autofill popups, especially relative to iframes or other browser windows, enabling spoofing or interaction issues. Clicks intended for one frame targeting Autofill in another (VRP: `1474196`).
*   **Cross-Origin Data Leakage:** Information from one origin's Autofill data being accessible to another origin, often involving iframes.
    *   **VRP Pattern (iframe Boundaries):** Bugs in how Autofill data is associated with frames or how suggestions are filtered based on origin, potentially allowing a cross-origin iframe to trigger or access Autofill suggestions meant for the parent or another frame. Incorrect `SubmittedOrigin` checks (VRP: `1301164`). Iframe focus issues leading to data leakage (VRP: `1474196`).
    *   **VRP Pattern (Password Manager Interaction):** Leaking password existence or username hints across origins via Autofill previews or interactions (VRP: `1487110`). See [password_manager.md](password_manager.md).
*   **Visibility Bypass (Information Leak):** Triggering Autofill suggestions for fields that are not geometrically visible to the user (e.g., positioned off-screen, zero opacity, covered by other elements), potentially leaking sensitive data fragments in the suggestion labels/sublabels if the popup appears near the trigger element's logical location (even if off-screen).
    *   **VRP Pattern (Missing Visibility Checks):** `AutofillAgent` (renderer) checks basic element properties (disabled, readonly, focusable) but **lacks explicit geometric visibility checks** before sending `AskForValuesToFill` IPC to the browser. The browser-side `BrowserAutofillManager` and `AutofillExternalDelegate` also don't seem to perform robust visibility checks using the received `caret_bounds` before showing the UI. Attackers can create invisible but focusable fields, trigger the IPC, and potentially have suggestions displayed based on the field's type, leaking data. (VRP: `1108181`, VRP2.txt#5228).
*   **Incorrect Data Filling/Saving:** Autofill data being filled into the wrong fields, or data being saved under the wrong origin/profile.
*   **Bypassing Security Prompts:** Circumventing prompts required for sensitive actions (e.g., CVC unmasking, virtual card enrollment).
*   **Interaction Requirement Bypass:** Triggering fills without the required user gesture (e.g., click) by spoofing the `trigger_source`.

## 3. Further Analysis and Potential Issues
*   **Input Protection:** **Crucially, investigate adding input protection (e.g., `InputEventActivationProtector`) to Autofill popups and prompts** to mitigate keyjacking/clickjacking when obscured by non-focus-stealing UI like FedCM or permission prompts (Related to VRP: `339481295`, `342194497`).
*   **Geometric Visibility Checks:** Where should robust visibility checks occur? Renderer (`AutofillAgent`) before sending IPC? Browser (`BrowserAutofillManager`) before generating suggestions? UI Delegate (`AutofillExternalDelegate`) before showing popup? Relying solely on the renderer seems insufficient. How can checks be performed reliably from the browser process?
*   **Occlusion Detection:** How does Autofill detect if its UI is obscured by other browser UI (PiP, FedCM, Permissions)? Is the detection robust? Does it correctly hide or disable interaction when obscured? (Related to VRP: `339481295`, `342194497`).
*   **iframe Origin Checks:** Deep dive into cross-origin iframe scenarios. How are `FormStructure::host_frame()`, `ContentAutofillDriver` data lifting (`Lift` function setting frame tokens/origins), and origin checks in `AutofillPopupControllerImpl` used to prevent cross-origin leaks or triggering? Are there edge cases (e.g., redirects, dynamic iframe manipulation) that bypass these? (Related to VRP: `1301164`, `1474196`).
*   **Focus Tracking:** Analyze how Autofill tracks focus changes between frames and browser windows (`FocusedElementChanged`, `DidCompleteFocusChangeInFrame`). Can focus be manipulated to cause Autofill suggestions for one frame to appear or be interacted with while another frame has focus? (Related to VRP: `1474196`).
*   **Trigger Source Validation:** How robustly is the `trigger_source` validated? Can a non-user-initiated event spoof a user click source?
*   **Password Manager Interaction:** Review how Password Autofill previews usernames or indicates saved passwords. Ensure this doesn't leak information across origins, especially considering potential timing attacks or UI state manipulation (Related to VRP: `1487110`).
*   **Data Storage Security:** How is Autofill data stored and encrypted? Are there risks related to profile switching or data access by other browser components?

## 4. Code Analysis
*   `components/autofill/content/renderer/autofill_agent.cc`: Implements `WebAutofillClient`. Handles renderer events (`TextFieldValueChanged`, `FocusedElementChanged`, etc.). Performs *initial* checks (enabled, readonly, focus, security context, value state) in `ShowSuggestions`. Sends `AskForValuesToFill` IPC. **Lacks explicit geometric visibility check.** Uses `FormTracker` to detect submissions.
*   `components/autofill/content/browser/content_autofill_driver.cc`: Handles IPC from `AutofillAgent`. Contains `Lift` function to add browser context (frame tokens, origins) to renderer data. Routes events to `BrowserAutofillManager`.
*   `components/autofill/core/browser/foundations/browser_autofill_manager.cc`: Core browser logic. Receives events from driver. Manages `FormStructure` cache. Implements `OnAskForValuesToFillImpl`. Calls `BuildSuggestionsContext` (checks security context, `autocomplete` attr). Calls `GetAvailable...Suggestions` helpers (address/payments) to generate suggestion list. Calls `external_delegate_->OnSuggestionsReturned`. **Does not appear to perform geometric visibility checks itself.**
*   `components/autofill/core/browser/ui/autofill_external_delegate.h`: Interface between manager and UI. Receives `OnQuery` and `OnSuggestionsReturned`. Stores `caret_bounds` likely for positioning. Doesn't seem to perform visibility checks itself.
*   `chrome/browser/ui/autofill/autofill_popup_controller_impl.cc`: Manages the Autofill popup UI, deciding what to show and handling selection. Origin filtering logic is critical here. Might perform final checks before showing UI?
*   `chrome/browser/ui/views/autofill/popup/autofill_popup_view.cc`: Desktop UI view for the Autofill suggestion popup. **Appears to lack `InputEventActivationProtector`.**
*   Other Autofill Views (e.g., `SaveCardBubbleViews`, `VirtualCardEnrollBubbleViews`): Need checks for input protection and obscuring issues.
*   `FormStructure`: Represents a parsed form, including field types and origin information (`host_frame()`, `source_url()`). Validity of this data across frames is key.
*   `AutofillTable`: Database interaction for storing/retrieving Autofill profiles.
*   Password Manager components (`PasswordManagerClient`, `PasswordFormManager`): Interaction points for password Autofill.

## 5. Areas Requiring Further Investigation
*   **Add Input Protection:** **Prioritize investigating the feasibility and impact of adding `InputEventActivationProtector` or similar mechanisms to Autofill UI elements.**
*   **Visibility Check Location:** Determine the best place(s) to add robust geometric visibility checks (Renderer? Manager? Delegate? UI Controller?).
*   **iframe Scenarios:** Design and test complex iframe scenarios (cross-origin, nested, sandboxed, dynamically added/removed) to probe origin checks and focus handling.
*   **Obscuring Interactions:** Test interactions where Autofill UI is obscured by various other browser elements (PiP, Permissions, FedCM, Extensions) and vice-versa, focusing on keyboard and click interactions.
*   **Trigger Source Spoofing:** Investigate ways to trigger Autofill with a "trusted" source without actual user interaction.
*   **Password Field Previews:** Analyze timing and conditions under which password Autofill might reveal information pre-fill.

## 6. Related VRP Reports
*   VRP: `339481295` ($3000): Autofill prompt can be obscured by FedCM bubble dialog (Context: VRP2.txt#7963)
*   VRP: `342194497` ($3000): Autofill prompt can be obscured by permission prompt (PEPC) (Context: VRP2.txt#6928)
*   VRP: `1474196`: Address form autofill triggers on wrong frame due to focus issue.
*   VRP: `1301164`: Autofill data leakage via cross-origin iframe.
*   VRP: `1487110`: Password manager hint leakage.
*   VRP: `1108181`: Autofill preview triggered for invisible fields. (Context: Missing visibility checks).
*   VRP2.txt#5228: Autofill preview shown for invisible fields via EyeDropper interaction. (Context: Missing visibility checks, potentially trigger source spoofing).

*(See also [fedcm.md](fedcm.md), [permissions.md](permissions.md), [password_manager.md](password_manager.md), [privacy.md](privacy.md))*
