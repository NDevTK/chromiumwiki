# Component: Web Share API

## 1. Component Focus
*   **Functionality:** Implements the Web Share API ([Spec](https://w3c.github.io/web-share/)), allowing web pages to invoke the native sharing capabilities of the underlying platform (OS or browser UI) to share data (text, URLs, files) with other applications.
*   **Key Logic:** Handling `navigator.share()`, validating input data (`ShareData`), interacting with the platform's sharing mechanism (`SharingService`), displaying the share sheet UI. Requires transient user activation.
*   **Core Files:**
    *   `components/sharing_hub/`: UI elements like the share sheet.
    *   `chrome/browser/share/`: Core browser-side logic.
        *   `share_features.cc`
        *   `sharing_service.cc`/`.h`: Service coordinating sharing actions.
        *   `share_attempt.cc`
    *   Platform-specific implementation (e.g., `chrome/browser/share/android/java/src/org/chromium/chrome/browser/share/`, `chrome/browser/ui/views/sharing_hub/`)
    *   `third_party/blink/renderer/modules/webshare/navigator_share.cc`: Renderer-side API implementation.

## 2. Potential Logic Flaws & VRP Relevance
*   **Policy Bypass (SameSite):** Using the share mechanism, potentially involving redirects or specific targets (like sharing to Chrome itself), to bypass SameSite cookie restrictions.
    *   **VRP Pattern (SameSite Bypass):** Sharing a URL via Web Share API, which then opens in Chrome, could cause cross-site requests to incorrectly include SameSite=Lax/Strict cookies (VRP2.txt#11901). See [privacy.md](privacy.md).
*   **Arbitrary File Access/Download:** Using Web Share with specific data types or targets to access local files or trigger unintended downloads.
    *   **VRP Pattern (UNC Path Download):** Sharing crafted data containing UNC paths (`\\server\share`) could potentially trigger file downloads or access attempts without proper checks (VRP2.txt#5730). See [downloads.md](downloads.md).
*   **UI Spoofing:** Misleading the user about the data being shared or the target application in the share sheet UI.
*   **Information Leaks:** Leaking sensitive data (e.g., shared text/URLs, target app list) through the sharing process.
*   **User Activation Bypass:** Triggering `navigator.share()` without the required user gesture.

## 3. Further Analysis and Potential Issues
*   **Data Validation:** How is the `ShareData` (text, url, files) validated and sanitized before being passed to the platform sharing mechanism? Are file paths or URLs checked for malicious schemes or patterns (like UNC paths - VRP2.txt#5730)?
*   **Platform Interaction (`SharingService`):** Analyze how the `SharingService` interacts with the native OS sharing APIs on different platforms (Android Intents, Windows Shell, macOS ShareKit). Are there platform-specific vulnerabilities?
*   **SameSite Cookie Handling:** When a shared URL is opened (potentially back in Chrome), how is the navigation context handled regarding SameSite cookies? Does it correctly treat the navigation as cross-site? (VRP2.txt#11901).
*   **Share Sheet UI Security:** Can the share sheet UI be spoofed or manipulated? Is the displayed preview accurate?
*   **User Activation:** Verify strict enforcement of transient user activation for `navigator.share()`.

## 4. Code Analysis
*   `NavigatorShare::share`: Renderer-side entry point. Checks user activation (`LocalFrame::HasTransientUserActivation`). Packages `ShareData`.
*   `SharingServiceImpl`: Browser-side service handling the share request. Calls platform-specific implementation.
*   Platform Share implementations (e.g., `ShareServiceImpl::ShareViaIntent` on Android): Handles constructing and launching native share intents/dialogs. Check data sanitization and intent construction.
*   `SharingHubBubbleController` / `SharingHubBubbleViewImpl`: Desktop share sheet UI.

## 5. Areas Requiring Further Investigation
*   **UNC Path Handling:** Confirm that sharing data containing UNC paths does not lead to unintended network access or downloads across all platforms (VRP2.txt#5730).
*   **SameSite Cookie Bypass:** Test various scenarios involving sharing URLs and selecting Chrome (or other browsers) as the target to see if SameSite cookies are incorrectly sent (VRP2.txt#11901).
*   **File Sharing Security:** Analyze the security model for sharing `File` objects. How are permissions handled?
*   **Error Handling:** Review error handling when sharing fails or is cancelled.

## 6. Related VRP Reports
*   VRP2.txt#11901 (SameSite bypass via sharing URL)
*   VRP2.txt#5730 (Sharing UNC path triggers SMB connection/download)

*(See also [privacy.md](privacy.md), [downloads.md](downloads.md), [intents.md](intents.md))*
