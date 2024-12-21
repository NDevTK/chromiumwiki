# Chromium Security Wiki

This wiki contains analysis of Chromium components and potential security vulnerabilities.

**Important Note:** Research findings should be added directly to the relevant wiki page, following the format below. Do not create separate files for research notes. **Also, please keep the security research tips below very detailed; do not shorten them.**  **Additionally, please keep the wiki page list ordered by VRP risk (highest payout first).**

**Format of Each Wiki Page:**

Each wiki page follows a consistent format:

1. **Component Focus:** The page clearly states the specific Chromium component(s) being analyzed (e.g., `content/browser/bluetooth/web_bluetooth_service_impl.cc`).

2. **Potential Logic Flaws:** A list of potential logic flaws or vulnerabilities is provided, along with a brief description of each issue and its potential impact.

3. **Further Analysis and Potential Issues:** This section provides a more detailed analysis of the identified issues, highlighting specific areas of concern within the codebase. Research notes, including files reviewed and key functions, are integrated here. This section also includes a summary of relevant CVEs and their connection to the discussed functionalities.

4. **Areas Requiring Further Investigation:** This section contains additional points for further investigation, identified during the analysis.

5. **Secure Contexts and [Component Name]:** This section explains the interaction between the component's functionalities and secure contexts, highlighting the importance of secure contexts in mitigating vulnerabilities.

6. **Privacy Implications:** This section discusses the privacy implications of the component's functionalities.

7. **Additional Notes:** This section contains any additional relevant information or findings.

**Note:** The original `network.md` and `rendering_engine.md` files have been deleted, as their content has been split into more specific wikis. The `service_workers.md` file has been updated to remove payment-related content, which is now in `service_worker_payments.md`. The `security_headers.md` file has been split into more specific wiki pages for each security header.

**Tips for Security Researchers:**

Based on the Chromium Vulnerability Reward Program (VRP) data you provided, prioritize investigating files with high reward payouts, as these often indicate critical vulnerabilities. The data reveals several key areas for focused investigation:

*   **Site Isolation (`site_isolation.md`):** Thoroughly investigate the `UrlInfo`, `SiteInfo`, `SiteInstance`, and `BrowsingInstance` classes, as well as the `NavigationRequest` class, for potential logic errors that could compromise site isolation. Pay close attention to how URLs are parsed, how origins are determined, and how process allocation decisions are made. Focus on functions like `GetPossiblyOverriddenOriginFromUrl`, `SiteInfo::DetermineProcessLockURL`, `SiteInfo::GetSiteForURLInternal`, `NavigationRequest::GetUrlInfo`, `NavigationRequest::GetOriginToCommit`, `NavigationRequest::GetOriginForURLLoaderFactoryBeforeResponse`, and `NavigationRequest::GetOriginForURLLoaderFactoryAfterResponse`.

*   **Tab Management (`tabs.md`, `drag_and_drop.md`):**  The exceptionally high rewards for files like `tab_strip_model.cc` ($53,357) and `tabs_api.cc` ($14,604) highlight the criticality of tab management security. Focus your research on cross-origin communication, race conditions, and extension interactions. The drag-and-drop functionality within the tab strip also presents a high-risk area. Thoroughly analyze data validation, event handling, and cross-origin interactions to prevent injection attacks and data corruption.

*   **Autofill (`autofill.md`):** The extremely high reward for `autofill_popup_controller_impl.cc` ($52,544) points to significant vulnerabilities in the autofill popup. Concentrate on data sanitization, data persistence, and form submission.

*   **Payments (`payments.md`):**  The substantial reward for `payment_request_sheet_controller.cc` ($16,326) indicates vulnerabilities in payment handling. Prioritize secure communication, data encryption, and data storage.

*   **Extensions (`extension_security.md`, `extensions_web_request_api.md`, `extensions_debugger_api.md`, `extensions_tabs_api.md`, `extensions_webrtc_audio_private_api.md`):**  High rewards for `tabs_api.cc` ($14,604) and `debugger_apitest.cc` ($15,309) highlight vulnerabilities in extension APIs and debugging. Focus on the permission model, sandbox bypasses, and API vulnerabilities. Also, pay close attention to the `extensions_web_request_api.md`, `extensions_debugger_api.md`, `extensions_tabs_api.md`, and `extensions_webrtc_audio_private_api.md` pages.

*   **WebRTC (`webrtc.md`):** The significant reward for `audio_debug_recordings_handler.cc` ($30,000) points to vulnerabilities in media handling. Focus on data stream integrity, media sanitization, and real-time communication security. Also, pay close attention to the `media_audio_debug_recordings_handler.md`, `media_web_contents_video_capture_device.md`, and `media_stream_dispatcher_host.md` pages.

*   **Blink (`blink_layout.md`, `blink_frame.md`, `blink_core.md`):** The large number of rewarded files in the Blink components suggests a wide range of potential vulnerabilities. Focus on JavaScript execution, DOM manipulation, and cross-origin resource loading.

*   **Network (`disk_cache.md`, `quic.md`, `websockets.md`, `http.md`):**  The high reward for `network_context.h` ($16,000) suggests vulnerabilities in network handling. Focus on protocol handling, cookie handling, and caching mechanisms. Also, pay close attention to the `http.md` page.

*   **Downloads (`downloads.md`):** High rewards associated with download management highlight the importance of secure file handling and resource management. Focus on file type validation, download path sanitization, and resource leaks.

*   **DevTools (`devtools.md`, `devtools_ui_bindings.md`):** The high rewards for `devtools_browsertest.cc` ($22,250) and `devtools_ui_bindings.cc` ($7,000) highlight the importance of secure DevTools implementation. Focus on unauthorized access, data leakage, command injection, and XSS vulnerabilities.

*   **Input Handling and Events:** Thoroughly analyze input handling functions, event dispatching mechanisms, and gesture processing for potential injection vulnerabilities, spoofing attacks, and race conditions. Pay close attention to how input events are routed, validated, and processed by different components, including the `InputEventRouter`, `RenderWidgetHostViewAndroid`, `RenderWidgetHostViewAura`, and `InputMethodManagerWrapper`. Consider edge cases and unexpected input sequences.

*   **UI and View Management:** Carefully review the implementation of UI elements, view hierarchies, and visual updates for potential spoofing or manipulation vulnerabilities. Focus on how UI components are rendered, updated, and interacted with. Analyze the handling of view properties, sizes, positions, and visibility. Pay attention to the `RenderWidgetHostViewAndroid`, `RenderWidgetHostViewAura`, and other view-related classes.

*   **Resource Loading and Scheduling:** Investigate the resource loading and scheduling mechanisms for potential vulnerabilities related to resource starvation, denial-of-service attacks, and race conditions. Analyze how resources are prioritized, queued, and fetched. Review the `ResourceLoadScheduler` and its interaction with other components. Consider the impact of different throttling policies and resource limits.

*   **Navigation and URL Handling:** Thoroughly analyze navigation handling, URL parsing, and redirect processing for potential vulnerabilities related to URL manipulation, cross-origin navigation, and history modification. Pay close attention to how URLs are validated, sanitized, and processed by different components, including the `NavigationPredictor`, `LocalFrameView`, and other navigation-related classes.

*   **Clipboard and Data Handling:** Carefully review clipboard interactions and data handling functions for potential data leakage, manipulation, or unauthorized access vulnerabilities. Analyze how clipboard data is read, written, and protected. Review input validation and sanitization in clipboard-related functions. Pay attention to the `Clipboard` API implementation and its interaction with other components.

*   **Password Management:** Review password management logic, including password generation, storage, and reuse detection, for potential vulnerabilities related to password leakage, weak password generation, or bypasses of security checks. Analyze the `PasswordManager`, `PasswordReuseController`, and other password-related components.

*   **Media Handling:** Investigate media handling components, such as the `VideoCaptureManager`, `AudioDebugRecordingsHandler`, `WebContentsVideoCaptureDevice`, and `MediaStreamDispatcherHost`, for potential vulnerabilities related to unauthorized access, data leakage, race conditions, and denial-of-service attacks. Analyze how media devices are accessed, how media streams are managed, and how captured data is handled.

*   **File System Access and Downloads:** Review file system access and download handling for potential vulnerabilities related to unauthorized file access, file type restriction bypasses, and malicious file execution. Analyze the `SelectFileDialogExtension`, `DownloadManager`, and other download-related components. Pay close attention to how file paths are handled, how file types are validated, and how downloads are initiated and managed.
