# Component: Picture-in-Picture (PiP)

## 1. Component Focus
*   **Functionality:** Implements the Picture-in-Picture (PiP) feature for both `<video>` elements ([Video PiP Explainer](https://github.com/WICG/picture-in-picture/blob/main/explainer.md)) and arbitrary HTML content ([Document PiP Explainer](https://github.com/WICG/document-picture-in-picture/blob/main/explainer.md)). Creates an always-on-top window displaying the PiP content.
*   **Key Logic:** Managing the PiP window lifecycle (`Show`, `Close`), handling media sessions (`MediaSessionImpl`), embedding surfaces (`EmbedSurface`), managing window controllers (`VideoPictureInPictureWindowControllerImpl`, `DocumentPictureInPictureWindowControllerImpl`), ensuring correct origin display and security boundaries for Document PiP, handling occlusion interactions with other browser UI.
*   **Core Files:**
    *   `content/browser/picture_in_picture/` (Core browser-side logic, e.g., `picture_in_picture_window_controller_impl.cc`)
    *   `chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.cc`: Implements the frame and UI (including the minimal address bar) for Document PiP. Contains `GetURL()` logic.
    *   `chrome/browser/picture_in_picture/`: Chrome-specific logic, e.g., `picture_in_picture_window_manager.cc`, `picture_in_picture_occlusion_tracker.cc`.
    *   `chrome/browser/ui/views/overlay/video_overlay_window_views.cc`: Video PiP UI implementation. Uses `ShowInactive()`.
    *   `third_party/blink/renderer/modules/picture_in_picture/`: Renderer-side API implementations.

## 2. Potential Logic Flaws & VRP Relevance
PiP features, especially Document PiP, have been sources of significant vulnerabilities due to their always-on-top nature and interactions with other UI elements and security boundaries.

*   **UI Obscuring (High VRP Frequency):** PiP windows, being always-on-top and sometimes interactable (`ShowInactive` used by Video PiP), can obscure other sensitive browser UI elements. If the obscured UI lacks robust input protection (like `InputEventActivationProtector`), this enables attacks.
    *   **VRP Pattern (Obscuring Prompts):** Video/Document PiP windows obscuring:
        *   Permission prompts (PEPC): Leads to granting permissions without awareness (VRP: `342194497`; VRP2.txt#6928).
        *   Autofill prompts: Leads to stealing autofill data (VRP: `40058582`; VRP2.txt#5228).
        *   FedCM prompts: Leads to hidden logins (VRP: `339654392`; VRP2.txt#12993).
        *   File Dialogs: Leads to spoofed file reads/writes via keyjacking (VRP2.txt#14537).
    *   **Mechanism:** Obscured prompts often remain interactive via keyboard if the PiP window doesn't steal focus (Video PiP uses `ShowInactive`). Lack of input protection on the obscured UI (e.g., Autofill, historically Permissions) is key. See [permissions.md](permissions.md), [autofill.md](autofill.md), [fedcm.md](fedcm.md).
    *   **VRP Pattern (Obscuring Fullscreen Toast):** PiP can obscure the fullscreen entry notification (VRP2.txt#7163).

*   **Origin Spoofing (Document PiP - High VRP Frequency):** Flaws in how the origin is determined (`GetURL` in `PictureInPictureBrowserFrameView`) and displayed for Document PiP windows, especially when initiated from subframes, after navigations/crashes, or using specific URL types. The core issue often lies in using the *opener's top-level WebContents* (`parent_web_contents` in `EnterDocumentPictureInPicture`) instead of the *PiP's own WebContents* (`child_web_contents`) to determine the displayed URL/origin.
    *   **VRP Pattern (iframe Initiator):** Document PiP address bar showing the top-level opener's origin instead of the PiP content's origin when opened from an iframe. (VRP: `40063068`; VRP2.txt#84, #10137).
    *   **VRP Pattern (FencedFrame Interaction):** Similar origin confusion when interacting with Fenced Frames. (VRP: `40062954`; VRP2.txt#7262). See [fenced_frames.md](fenced_frames.md).
    *   **VRP Pattern (Spoof via `opener`):** Manipulating `window.opener` relationship to cause origin confusion. (VRP: `40062959`; VRP2.txt#304).
    *   **VRP Pattern (URL Formatting/Parsing):** Using long `about:blank#...` URLs (VRP2.txt#4130) or potentially `javascript:` URLs (VRP2.txt#15301) to manipulate the displayed origin in the PiP window.
    *   **VRP Pattern (Navigation/Crash Timing):** Opening PiP during opener navigation or after a crash can lead to stale origin information being displayed. (VRP2.txt#10177). See [navigation.md](navigation.md).
    *   **VRP Pattern (Extension Popup Initiator):** Opening PiP from an extension popup incorrectly inherits the origin of the active tab instead of the extension origin (VRP2.txt#14228).

*   **Input/Interaction Issues:** Compromised renderers manipulating PiP window state or interaction.
    *   **VRP Pattern (Resize/Move):** Document PiP window could historically be resized/moved by a compromised renderer, potentially aiding UI obscuring attacks or making spoofing more effective. (VRP: `40063071`; VRP2.txt#310).

## 3. Further Analysis and Potential Issues
*   **Occlusion Handling & Input Protection:** How does the PiP implementation interact with occlusion detection systems (`PictureInPictureOcclusionTracker`) used by other UI elements (Autofill, Permissions, FedCM, File Dialogs)? **Is the protection robust against all PiP types (Video vs. Document) and initiation methods?** The use of `ShowInactive()` by Video PiP remains a concern. Ensure obscured UI elements consistently ignore input.
*   **Document PiP Origin Determination:** Audit the logic in `PictureInPictureBrowserFrameView::GetURL()` and the call chain leading to `PictureInPictureWindowManager::EnterDocumentPictureInPicture`. **Ensure the `child_web_contents` (the PiP content) is the source for URL/origin display**, not `parent_web_contents` (the opener tab). Verify this holds for all initiation contexts (top-level, iframe, extension popup, after navigation/crash).
*   **Window Management & Focus:** Analyze how PiP windows are managed relative to other browser windows (focus state, always-on-top status, closing behavior when opener closes/navigates). Can focus be manipulated to bypass protections?
*   **Interaction with Sandboxed Frames/Fenced Frames:** Thoroughly test how PiP behaves when initiated from sandboxed iframes or Fenced Frames. Are origin and permission checks correctly handled throughout the lifecycle? (VRP: `40062954`).

## 4. Code Analysis
*   `VideoPictureInPictureWindowControllerImpl::Show()`: Calls `window_->ShowInactive()`. Key enabler for UI obscuring attacks by Video PiP as it doesn't take focus from the main window (related to VRP: `40058582`).
*   `DocumentPictureInPictureWindowControllerImpl`: Manages Document PiP window lifecycle.
*   `PictureInPictureBrowserFrameView`: Implements the frame view for Document PiP.
    *   `GetURL()`: Determines the URL displayed in the PiP title/address bar. **Historically susceptible to spoofing by using the wrong WebContents data source** (VRP: `40063068`, VRP2.txt#84, #10137, #14228, #4130, #10177). Needs careful audit to ensure it uses the *PiP's* content origin.
*   `PictureInPictureWindowManager`: Manages PiP controllers and window state.
    *   `EnterDocumentPictureInPicture(parent_web_contents, child_web_contents)`: Handles creation. **Historically passed `parent_web_contents` (opener tab) incorrectly to logic that determined the displayed origin**, instead of using `child_web_contents` (PiP content). (Root cause for many origin spoofs like VRP: `40063068`).
*   `PictureInPictureOcclusionTracker`: Responsible for tracking PiP windows and determining if they occlude other UI. Its effectiveness with complex UI layouts (e.g., PEPC via scrim - VRP: `342194497` context) needs verification.
*   Interaction with `AutofillPopupControllerImpl`, `PermissionPromptBaseView`, `FedCmAccountSelectionView`, File Dialogs (`SelectFileDialog`): Check how these components detect occlusion by PiP windows and whether their input protection is sufficient.

## 5. Areas Requiring Further Investigation
*   **Robust Occlusion Mitigation:** Ensure all sensitive UI elements (Autofill, Permissions, FedCM, Download shelf, File Dialogs, etc.) have robust mechanisms to detect and prevent interaction when obscured by *any* type of PiP window, regardless of focus state (`ShowInactive`). **Prioritize adding `InputEventActivationProtector` or equivalent to Autofill, Permissions, FedCM UI.**
*   **Document PiP Origin Consistency:** **Critically review and refactor Document PiP origin display logic (`GetURL()`, `EnterDocumentPictureInPicture` usage) to consistently use the origin of the content *within* the PiP window (`child_web_contents`), not the opener's (`parent_web_contents`)**, in all scenarios (iframe, extension popup, crash, nav, long URLs, special schemes).
*   **Fenced Frame Interactions:** Thoroughly test interactions between PiP (especially Document PiP) and Fenced Frames for potential origin confusion or policy bypasses (VRP: `40062954`).
*   **Window Resize/Move Restrictions:** Verify that compromised renderers can no longer arbitrarily resize/move PiP windows (fix for VRP: `40063071`).

## 6. Related VRP Reports
*   **UI Obscuring:**
    *   VRP: `342194497` / VRP2.txt#6928 (Obscures PEPC)
    *   VRP: `40058582` / VRP2.txt#5228 (Obscures Autofill)
    *   VRP: `339654392` / VRP2.txt#12993 (Obscures FedCM)
    *   VRP2.txt#14537 (Obscures File Dialog)
    *   VRP2.txt#7163 (Obscures Fullscreen Toast)
*   **Origin Spoofing (Document PiP):**
    *   VRP: `40063068` / VRP2.txt#84, #10137 (From iframe)
    *   VRP: `40062954` / VRP2.txt#7262 (with Fenced Frame)
    *   VRP: `40062959` / VRP2.txt#304 (via opener)
    *   VRP: `1429246` (From iframe)
    *   VRP: `1450728` (From iframe)
    *   VRP2.txt#13133 (From iframe)
    *   VRP2.txt#14228 (From extension popup)
    *   VRP2.txt#4130 (long about:blank URL)
    *   VRP2.txt#10177 (post-crash/nav)
    *   VRP2.txt#15301 (via javascript: URL)
*   **Interaction/Manipulation:**
    *   VRP: `40063071` / VRP2.txt#310 (Resize/Move by compromised renderer)

## 7. Cross-References
*   [autofill.md](autofill.md)
*   [permissions.md](permissions.md)
*   [fedcm.md](fedcm.md)
*   [fenced_frames.md](fenced_frames.md)
*   [navigation.md](navigation.md)
