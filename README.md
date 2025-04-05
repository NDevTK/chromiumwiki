# Chromium Security Wiki

This wiki contains analysis of Chromium components and potential security vulnerabilities.

**Important Notes:**

*   **Dynamic Prioritization:** The order of wiki pages should initially reflect a combination of perceived bug likelihood and potential VRP reward (highest potential value first). **However, this order is dynamic and should be adjusted based on new information.** For instance:
    *   If a codebase search reveals specific patterns or potentially vulnerable functions related to a topic, its priority might increase.
    *   If significant time is spent investigating an area with little progress, its priority might decrease. Conversely, making good progress can increase priority.
    *   As new vulnerability types or attack vectors are discovered (internally or externally), related areas should be reprioritized.
*   **Continuous Updates:** Research findings, analysis, potential issues, and relevant VRP examples should be added *directly* to the relevant wiki page following the format below. **Do not create separate files for research notes.** As new ideas, findings, or VRP data emerge, **ensure the corresponding wiki pages are updated promptly** to reflect the latest understanding.
*   **Detail is Key:** Please keep the security research tips below very detailed, drawing on examples from the VRP data; do not shorten them.

**Format of Each Wiki Page:**

Each wiki page follows a consistent format:

1.  **Component Focus:** Clearly state the specific Chromium component(s) being analyzed (e.g., `content/browser/renderer_host/render_widget_host_view_aura.cc`, `components/autofill/content/renderer/autofill_agent.cc`).
2.  **Potential Logic Flaws & VRP Relevance:** List potential logic flaws or vulnerabilities. Briefly describe each issue, its potential impact, and cite relevant VRP examples if applicable (e.g., "Origin confusion in DevTools - See VRP #12345").
3.  **Further Analysis and Potential Issues:** Provide a more detailed analysis of identified issues, highlighting specific areas of concern within the codebase. Integrate research notes (files reviewed, key functions). Summarize relevant CVEs/VRP reports and their connection to the discussed functionalities.
4.  **Code Analysis:** Include specific code snippets and observations relevant to potential vulnerabilities.
5.  **Areas Requiring Further Investigation:** List specific questions, functions, or interactions needing deeper analysis.
6.  **Related VRP Reports:** Link to specific VRP reports (like those in `VRP.txt` and `VRP2.txt`) that demonstrate vulnerabilities in this or related components.

**General Security Research Tips:**

This section provides actionable tips for security research in Chromium, informed by common vulnerability patterns observed in VRP reports.

**1. UI Security: Spoofing & Obscuring (High Likelihood, High VRP Value):**

*   **Focus:** Examine UI rendering logic, especially for security indicators (address bar, permission prompts, dialogs). Look for spoofing, obscuring, bypassing, or clickjacking vulnerabilities. Pay attention to interactions between UI elements.
*   **Techniques & VRP Patterns:**
    *   **Address Bar Spoofing:** Analyze omnibox display logic, especially on Android. Look for issues with long URLs, special schemes (`blob:`, non-http/s), delayed scheme display, or interaction with scrolling/tab switching (VRP: 40072988, 379652406, 343938078, 40064170, 40057561). Check URL formatting and elision (VRP: 40061104). See [url_formatting.md](url_formatting.md).
    *   **Dialog/Prompt Obscuring:** Test interactions where one UI element can overlay another sensitive one. Examples: Picture-in-Picture (Video/Document) obscuring Permissions (PEPC), Autofill, FedCM prompts (VRP: 40058582, 342194497, 339654392). Extension popups obscuring permissions/screen share prompts (VRP: 40058873). FedCM bubbles obscuring Autofill prompts (VRP: 339481295, 340893685). Share dialogs rendering over browser UI (VRP: 40056848). Pop-up blocker notification overlapping fullscreen toast (VRP: issue 1218061 - implies cross-notification interaction). Android keyboard overlapping fullscreen toast (VRP: 1270988). Android text selection menu overlapping fullscreen toast (VRP: 1417137). See [fedcm.md](fedcm.md), [picture_in_picture.md](picture_in_picture.md), [permissions.md](permissions.md).
    *   **Dialog/Prompt Spoofing:** Investigate PWA install prompts (origin display), external protocol dialogs (redirects), and chooser dialogs (Bluetooth/USB/Serial/HID) for origin confusion, especially with opaque origins or permission delegation (VRP: 40055515, 40061374, 40061373). Check if prompts render outside initiator windows (VRP: 341663594, 338233148). Check if prompts can be overlaid over other origins (VRP: 1404001, 1372911). Check if prompts can be accepted without visibility (VRP: 1371215). See [web_app_identity.md](web_app_identity.md), [webserial.md](webserial.md), [webusb.md](webusb.md), [bluetooth.md](bluetooth.md).
    *   **Input/Interaction Hijacking:** Investigate techniques like custom cursors overlaying UI (VRP: 40057147, 1381087, 1376859), keyjacking focused prompts/dialogs (PaymentRequest - VRP: 1403539, Permissions - VRP: 1371215), or confusing cursor position (EyeDropper - VRP: 1466230). See [permissions.md](permissions.md), [payments.md](payments.md), [eye_dropper.md](eye_dropper.md).
    *   **Fullscreen Issues:** Lack of fullscreen notification (VRP: 1311683, 1375785 - landscape, 1317234 - repeated exit/enter, 1270988 - overlaps, 1264561 - cancel), ability to prevent exit (Esc key blocked by Share API - VRP: 1467734, Keyboard Lock - VRP: 1385387, JS loops - VRP: 1453882).
*   **Specific Areas:** Omnibox ([omnibox.md](omnibox.md)), Permission Prompts (PEPC) ([permissions.md](permissions.md)), Autofill UI ([autofill_ui.md](autofill_ui.md)), Download UI ([downloads.md](downloads.md)), PaymentRequest UI ([payments.md](payments.md)), FedCM UI ([fedcm.md](fedcm.md)), Web Share UI ([webshare.md](webshare.md)), Picture-in-Picture UI ([picture_in_picture.md](picture_in_picture.md)), Fullscreen Notifications, Custom Cursors ([input.md](input.md)?), Chooser Dialogs ([bluetooth.md](bluetooth.md), [webusb.md](webusb.md), [webserial.md](webserial.md)), PWA Install Prompts ([web_app_identity.md](web_app_identity.md)), External Protocol Dialogs.

**2. Extension & DevTools Security (High Likelihood, High VRP Value):**

*   **Focus:** Analyze extension API security boundaries, permission models, and interactions with privileged browser functions or pages. Look for sandbox escapes, policy bypasses, and unintended API usage, particularly involving DevTools. See [extension_security.md](extension_security.md).
*   **Techniques & VRP Patterns:**
    *   **DevTools Interaction:** Exploiting `chrome.debugger` API for sandbox escapes (VRP: 40056776, 40060173, 40060283, 1121192, 1113558, 1059676, 798222, 798184) via methods like `Page.navigate` (to file://, privileged URLs), `Page.captureSnapshot`/`Page.captureScreenshot` (local file read - VRP: 1116444, 1116445, 1385343, 1409564), `Input.synthesizeTapGesture`, `Input.dispatchKeyEvent`, `Target.setAutoAttach`, `Target.sendMessageToTarget`. Race conditions during navigation or attachment (VRP: 1328108). Insufficient checks in `chrome.devtools.inspectedWindow.reload` (VRP: 41483638, 1473995, 1059577). See [extensions_debugger_api.md](extensions_debugger_api.md).
    *   **Privileged Page Interaction:** Gaining script execution on `chrome://`, `devtools://`, or Web Store pages (VRP: 41483638, 1473995, 1490106, 1059676, 798184, 797497, 797500). Using WebUI message handlers insecurely (`chrome://policy` - VRP: 41483638). See [devtools.md](devtools.md).
    *   **Policy Bypass:** Circumventing `runtime_blocked_hosts` (VRP: 40060283, 41493344) or other enterprise policies. See [policy.md](policy.md).
    *   **Window Manipulation:** Obscuring active windows with inactive ones (VRP: 40058935), moving windows off-screen (VRP: 40058916) to enable keyboard interaction without user awareness. See [extensions_app_window.md](extensions_app_window.md)?
    *   **Permission Escalation:** Leaking information or gaining capabilities beyond granted permissions (e.g., reading local files with only `downloads` - VRP: 1377165, 1385343, 1409564, 989078, reading other extension storage via `webview` `loadDataWithBaseUrl` - VRP: 1116448). Incorrect permission checks during updates (VRP: 1418104). Leaking tab info via `chrome.tabs.onUpdated` (VRP: 1306167). See [permissions.md](permissions.md).
    *   **CSP Bypass:** Exploiting CSP validator flaws (`script-src-elem`/`script-src-attr` - VRP: 1288035). See [content_security_policy.md](content_security_policy.md).
    *   **Side Panel Interaction:** Exploiting interactions with privileged side panel UIs (e.g., Companion/Lens - VRP: 1482786). See [side_panel.md](side_panel.md)?
*   **Specific Areas:** `chrome.debugger` API ([extensions_debugger_api.md](extensions_debugger_api.md)), `chrome.devtools` APIs ([devtools.md](devtools.md)), `chrome.downloads` API (esp. `onDeterminingFilename`) ([downloads.md](downloads.md)), `chrome.tabs` API ([extensions_tabs_api.md](extensions_tabs_api.md)), `webview` tag, `chrome.storage`, `chrome.runtime.sendMessage`, Content Scripts ([extensions_content_verifier.md](extensions_content_verifier.md)?), Extension Manifest (CSP, permissions, `devtools_page`), `chrome://` pages (policy, downloads, flags, settings), `devtools://devtools`, Chrome Web Store pages.

**3. Autofill & Input Handling Bypasses (High Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate bypasses for autofill security measures (interaction requirements, visibility checks). Analyze input methods, timing attacks, and interactions with other browser features. Look for regressions. See [autofill.md](autofill.md).
*   **Techniques & VRP Patterns:**
    *   **Input Method Abuse:** Bypasses using EyeDropper API (VRP: 40065604, 40063230, 40058496), taps (double-taps, rendering near/under cursor - VRP: 40060134, 40058217, 40056900, 1426679, 1487440), space key input (VRP: 40056936), pointer lock (VRP: 40056870), or physical keyboard accessory (VRP: issuetracker.google.com/181313978). See [eye_dropper.md](eye_dropper.md), [pointer_lock.md](pointer_lock.md), [input.md](input.md)?
    *   **Visibility Bypass:** Obscuring autofill prompt with PiP windows (VRP: 40058582), FedCM dialogs (VRP: 339481295, 340893685). Tricking visibility checks by manipulating input field cache or display (VRP: 1395164, 1358647, 1108181). Docking/clipping prompt in small windows/iframes (VRP: 1395164 - Android). See [autofill_ui.md](autofill_ui.md), [picture_in_picture.md](picture_in_picture.md), [fedcm.md](fedcm.md).
    *   **Timing/State Issues:** Exploiting delays in prompt rendering after `mousedown` by moving the input field (VRP: 40058217, 40056900). Exploiting preview state text leaks (`scrollWidth`, font manipulation via `::first-line` or `@font-face` override - VRP: 1035058, 1035063, 1013882, 951487, 916838). Leaking selection via `scrollTop` in `<select>` (VRP: 1250850).
    *   **Regressions:** Autofill bypasses frequently reappear after refactoring or fixes for related issues (VRP: 40063230 - regression of 1287364).
*   **Specific Areas:** Autofill prompt logic (`AutofillPopupView`, input event handling), EyeDropper API ([eye_dropper.md](eye_dropper.md)), PaymentRequest API ([payments.md](payments.md)), Pointer Lock API ([pointer_lock.md](pointer_lock.md)), `<select>` element handling, interactions with PiP/FedCM.

**4. IPC/Mojo Security (Medium Likelihood, High VRP Value - Sandbox Escape Focus):**

*   **Focus:** Deeply analyze IPC messages and Mojo interfaces, especially those callable from less privileged contexts (renderer, extensions). Look for insufficient validation, access control flaws, memory safety issues (UAF in `observer_list.h` - VRP: 40061678), and incorrect state handling. See [ipc.md](ipc.md), [mojo.md](mojo.md).
*   **Techniques & VRP Patterns:**
    *   **Insufficient Validation/Access Control:** Lack of checks on parameters or caller privileges (e.g., `StartDragging` allowing arbitrary mouse control - VRP: `VRP2.txt#1`). Incorrect handling of messages (e.g., Browser handling renderer-intended `ACCEPT_BROKER_CLIENT` - VRP: VRP2.txt#370). Lack of origin checks (e.g., `PushMessaging` API - VRP: 1275626; `ContentIndex` - VRP: 1263530, 1263528).
    *   **Privilege Escalation:** Gaining higher privileges via compromised renderer sending crafted messages.
    *   **Sandbox Escapes:** Directly escaping the sandbox through vulnerable IPC channels.
    *   **State Confusion:** Exploiting race conditions or dangling pointers in IPC state management (e.g., `PermissionRequestManager` - VRP: 1424437).
*   **Relevant Files/Interfaces:** `mojo/public/interfaces/bindings/native_struct.mojom`, `mojo/public/cpp/bindings/message.h`, `mojo/core/node_channel.cc`, `IProcessLauncher`/`GoogleUpdate.ProcessLauncher` (VRP: 340090047), interfaces related to file system access, user input simulation, privileged UI, permissions, DevTools.

**5. Web API & Feature Security (Medium Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate newer or complex web platform features, paying attention to their security models, interactions, and potential for origin confusion or policy bypass. Also consider older but complex features.
*   **Techniques & VRP Patterns:**
    *   **Origin Spoofing/Confusion:** Dialog origin issues related to Web APIs (Bluetooth, USB, Serial, HID - VRP: 40061374, 40061373). FedCM prompts with opaque origins (VRP: 340893685). Document PiP origin issues (VRP: 40063068, 40062959, 1429246, 1450728). FencedFrames interaction with PiP (VRP: 40062954). Portal API URL spoof after crash (VRP: 40064170). External protocol dialogs via redirects (VRP: 40055515). javascript:/data: URL origin confusion (VRP: 40059251, VRP2.txt#173). Blob URL origin issues (VRP: 1069246, 705778). See [webid.md](webid.md), [fedcm.md](fedcm.md), [picture_in_picture.md](picture_in_picture.md), [fenced_frames.md](fenced_frames.md), [portals.md](portals.md).
    *   **Policy Bypass (CSP, SameSite, Sandbox):** SameSite cookie bypasses (Web Share API VRP: issuetracker.google.com/303661203; BackgroundFetch VRP: 1244289; redirects VRP: 698492; Service Worker FetchEvent VRP: 1115438; Android Intents VRP: 1375132). CSP bypasses (Service Worker intercept VRP: 598077; `about:blank` navigation VRP: 996741; `blob:` navigation VRP: 1115628; `filesystem:` navigation VRP: 1116446; `javascript:` in `srcdoc` VRP: 1006188). Iframe Sandbox bypasses (`allow-popups-to-escape-sandbox` VRP: 40069622, 40057525; `allow-downloads` VRP: 40060695; `intent://` URLs VRP: 1365100; `javascript:` links with `window.opener` VRP: 1017879). See [content_security_policy.md](content_security_policy.md), [iframe_sandbox.md](iframe_sandbox.md), [service_workers.md](service_workers.md), [background_fetch.md](background_fetch.md).
    *   **Information Leaks:** Cross-origin URL leaks (Fetch API `no-cors` on iOS VRP: 803830; redirects in Performance API VRP: 40054148; WebGL errors VRP: 658029; redirects in `<script>` errors/CSP reports VRP: 1087902). Cross-origin size leaks (BackgroundFetch VRP: 1247376, 1260649, 1267311; redirects/Range requests VRP: 990849, 1260649, 1270469). Cross-origin pixel data/timing leaks (Canvas `drawImage` VRP: 1093099, 781017, 686498; CSS filters VRP: 716057). History/Visited link leaks (SoftNav+paint VRP: 1459093; CSS Paint API VRP: 680214; CSS transitions VRP: 1211002; `history.length` VRP: 1208614; `document.baseURI` VRP: 1329654). Environment variable leaks (`showSaveFilePicker`/`downloads.download` VRP: 1247389, 1310461, 1322058, 1310462). Keystroke timing leaks (VRP: 1315899). SharedWorker cross-origin access (VRP: 670211). See [performance_apis.md](performance_apis.md).
    *   **API Misuse/Interaction:** PaymentRequest API bypasses (VRP: 40072274). Push Messaging API flaws (VRP: 1275626). Content Index API origin checks (VRP: 1263530, 1263528). Scroll inference via Text Fragments (VRP: 1400345). File System Access API interaction with downloads (VRP: 1428743). Reading local files via `registerProtocolHandler` (VRP: 971188). See [push_messaging.md](push_messaging.md), [file_system_access.md](file_system_access.md).
*   **Specific APIs:** WebTransport, WebCodecs ([webcodecs.md](webcodecs.md)), WebGPU ([webgpu.md](webgpu.md)), WebXR ([webxr.md](webxr.md)), FedCM ([fedcm.md](fedcm.md)), Web Share API ([webshare.md](webshare.md)), Web Bluetooth ([bluetooth.md](bluetooth.md)), Web USB ([webusb.md](webusb.md)), Web Serial ([webserial.md](webserial.md)), Portals ([portals.md](portals.md)), File System API ([file_system_access.md](file_system_access.md)), Push Messaging API ([push_messaging.md](push_messaging.md)), Translate API ([translation_ui.md](translation_ui.md)?), PaymentRequest API ([payments.md](payments.md)), BackgroundFetch API ([background_fetch.md](background_fetch.md)), Content Index API, Text Fragments, FencedFrames ([fenced_frames.md](fenced_frames.md)), Service Workers ([service_workers.md](service_workers.md)), SharedWorker ([worker_threads.md](worker_threads.md)?), `registerProtocolHandler`.

**6. Filesystem & Scheme Handling (Medium Likelihood, Variable VRP Value):**

*   **Focus:** Analyze how the browser handles file interactions, special URL schemes, and downloads, especially regarding security boundaries and information leakage.
*   **Techniques & VRP Patterns:**
    *   **Local File Access/Disclosure:** Reading local files via extensions (DevTools interaction VRP: 1116445, 1116444, 1385343, 1409564; `downloads` permission + FSA VRP: 1428743; `input[type=file]` + symlinks VRP: 1378484, 1381634; `sourceMappingURL` VRP: 1349146, 1419604, 1429241; `registerProtocolHandler` VRP: 971188; `.url` files VRP: 1303486; `SharedWorker` VRP: 670211). Information disclosure via file picker defaulting (`input[type=file]` reading all files VRP: 756268). Android private data access via intents (VRP: 1198142). FencedFrames loading local directories (VRP: 1454937). See [downloads.md](downloads.md), [file_system_access.md](file_system_access.md).
    *   **Special Scheme Handling:** Bypasses involving `intent://` (VRP: 40064598, 1365100, 1375132), `android-app://` (VRP: 1092025), `javascript:` (Origin confusion VRP: 40059251, VRP2.txt#173; CSP bypass VRP: 1006188; Self-XSS VRP: 850824, 738694), `file://` (Navigation bypasses VRP: 40060173; Reading via extensions VRP: see above; UNC paths in `sourceMappingURL` VRP: 1342722, 1379985), `chrome-untrusted://` (XSS via parsing VRP: 40057777), `fido://` (Deep link hijacking VRP: issuetracker.google.com/issues/370176231). See [intents.md](intents.md)?
    *   **Download Security:** Origin spoofing in download UI (VRP: 1157743, 1281972, 1499408, 916831). Masking file types (e.g., `.jpg.scf` VRP: 1228653; `%%` bypass VRP: 1378895). Bypassing dangerous file checks via DevTools (VRP: 798217). Bypassing Safe Browsing via large data URIs (VRP: 1416794). Silent downloads/overwrites (`showSaveFilePicker` + Enter key VRP: 1243802; Web Share API VRP: 1297692). Sandbox bypass (`allow-downloads` VRP: 40060695; `noopener` downloads VRP: 1105523). History manipulation leading to fake downloads (VRP: 1513412). See [downloads.md](downloads.md), [safe_browsing_service.md](safe_browsing_service.md).
*   **Specific Areas:** File Input (`<input type="file">`), File System Access API ([file_system_access.md](file_system_access.md)), Download UI & logic (`chrome.downloads`, Safe Browsing integration), `registerProtocolHandler`, URL parsing/handling for various schemes ([url_utilities.md](url_utilities.md)?), `sourceMappingURL` handling.

**7. Installer/Updater Security (Windows/macOS) (Medium Likelihood, High VRP Value - EoP Focus):**

*   **Focus:** Analyze installer and updater components (e.g., Google Update Service, Keystone) for privilege escalation vulnerabilities, especially focusing on file operations, registry interactions, and IPC/COM interfaces on Windows/macOS. See [installer_security.md](installer_security.md).
*   **Techniques & VRP Patterns:**
    *   **Insecure File Operations:** Arbitrary file creation/write/delete in user-writable or predictable locations (`C:\Windows\Temp\`, `%APPDATA%\Local`, `C:\ProgramData\Google\Update\Log`, `/tmp/`) often exploitable via symlinks/hardlinks/junctions (VRP: 1317661, 1183137, 704138, VRP2.txt#1259, VRP2.txt#4175, VRP2.txt#1481). Race conditions during file caching or updates (VRP: 1152971). Lack of symlink checks during extraction/installation (VRP: VRP2.txt#914).
    *   **Insecure Registry Operations (Windows):** Deleting symlinked registry keys leading to deletion of protected keys (`HKCU\Software\Policies`) (VRP: VRP2.txt#8739).
    *   **Insecure IPC/COM (Windows):** COM objects accessible to low-privilege users exposing dangerous methods (`GoogleUpdate.ProcessLauncher` + Session Moniker VRP: 340090047).
    *   **Code Execution/Logic Flaws:** Lack of code signing checks during updates/installation (VRP: VRP2.txt#914). Incorrect permission inheritance or handling (`AlwaysInstallElevated` VRP: VRP2.txt#8739).
*   **Specific Components:** Google Update Service (Windows), Keystone (macOS), Chrome/Chromium Installer/Uninstaller (Windows/macOS), Crashpad installer actions. Relevant paths: `C:\Windows\Temp\`, `%APPDATA%\Local`, `C:\ProgramData\Google\Update\Log`, `/tmp/com.google.Keystone`, `C:\Program Files (x86)\Google\Update\Download`.

**8. General Code Analysis Techniques:**

*   **Fuzzing:** Use fuzzers (e.g., libFuzzer) to test APIs, file parsers (PDF - VRP: 40059101), IPC interfaces, and complex media C++ codecs (WebCodecs etc.). Focus on edge cases and stateful interactions.
*   **Code Review:** Manually review code changes, especially in security-sensitive areas (IPC/Mojo, process isolation, permissions, input validation, scheme handling, UI rendering). Look for common C++ vulnerabilities (TOCTOU, overflows, UAF - VRP: 40061678, type confusion - VRP: 1337607), logic flaws, and insecure assumptions. Check error handling and resource management (leaks).
*   **Variant Analysis:** After finding a vulnerability, search for similar patterns across the codebase using tools like CodeQL or manual grep/search. Look for incomplete fixes or related logic flaws.
*   **Exploit Knowledge:** Study public exploit techniques (e.g., Windows EoP via file operations, browser exploitation techniques) to inform code review and identify potential weaknesses.
*   **Corner Cases & Regressions:** Explicitly test edge cases, error conditions, and interactions between features. Verify that previously fixed bugs (especially in complex areas like Autofill) have not regressed.
*   **Platform-Specific Testing:** Test on various platforms (Windows, macOS, Linux, Android, ChromeOS, iOS), paying attention to platform-specific APIs, behaviors (e.g., Android Intents, macOS Keychain/XPC), and UI paradigms. Test Android WebView specifically.

**Wiki Pages (Prioritized List - *Adjust Dynamically Based on Research*):**

This list prioritizes components based on a combination of VRP value and frequency. Links point to existing or newly created pages.

*   **High VRP / High Frequency:**
    *   Chromium > Internals > Sandbox > SiteIsolation ([site_isolation.md](site_isolation.md)) - VRP: $20,000
    *   Chromium > Installer & Updater (Windows/macOS) ([installer_security.md](installer_security.md)) - VRP: $20,000+ (implied by multiple EoP reports)
    *   Chromium > Mobile > WebView ([android_webview_app_defined_websites.md](android_webview_app_defined_websites.md)) - VRP: $15,000
    *   Chromium > UI > Browser > Autofill > Payments ([payments.md](payments.md)) - VRP: $10,000
    *   Chromium > UI > Browser > Omnibox ([omnibox.md](omnibox.md)) - VRP: $8,500
    *   Chromium > Extensions ([extension_security.md](extension_security.md)) - VRP: $7,000+
    *   Chromium > UI > Browser > WebApps ([web_app_identity.md](web_app_identity.md)) - VRP: $5,000
    *   Chromium > Blink > Forms > Autofill ([autofill.md](autofill.md)) - VRP: $5,000
    *   Chromium > Blink > Media > PictureInPicture ([picture_in_picture.md](picture_in_picture.md)) - VRP: $4,000
    *   Chromium > UI > Browser > Permissions > Prompts ([permissions.md](permissions.md)) - VRP: $4,000
*   **Medium VRP / Medium Frequency:**
    *   Chromium > Blink > SecurityFeature > IFrameSandbox ([iframe_sandbox.md](iframe_sandbox.md)) - VRP: $3,000
    *   Chromium > Blink > Identity > FedCM ([fedcm.md](fedcm.md)) - VRP: $3,000
    *   Chromium > Platform > Extensions > API ([extensions_api.md](extensions_api.md)?) - VRP: $3,000
    *   Chromium > Mobile > Intents ([intents.md](intents.md)) - VRP: $3,000
    *   Chromium > Blink > Input > PointerLock ([pointer_lock.md](pointer_lock.md)) - VRP: $3,000
    *   Chromium > Blink > Serial ([webserial.md](webserial.md)) - VRP: $3,000
    *   Chromium > Privacy ([privacy.md](privacy.md)?) - VRP: $3,000
    *   Chromium > Blink > USB ([webusb.md](webusb.md)) - VRP: $3,000
    *   Chromium > Blink > SecurityFeature > COOP ([coop.md](coop.md)) - VRP: $3,000
    *   Chromium > Blink > Forms > Color ([color_input.md](color_input.md)?) - VRP: $2,000
    *   Chromium > UI > Browser > Navigation ([navigation.md](navigation.md)?) - VRP: $2,000
    *   Chromium > Blink > Input ([input.md](input.md)?) - VRP: $2,000
    *   Chromium > Blink > Portals ([portals.md](portals.md)) - VRP: $2,000
    *   Chromium > UI > Browser > SafeBrowsing ([safe_browsing_service.md](safe_browsing_service.md)) - VRP: $2,000
*   **Lower VRP / Lower Frequency:**
    *   Chromium > Blink > DataTransfer ([drag_and_drop.md](drag_and_drop.md)?) - VRP: $1,000
    *   Chromium > UI > Security > UrlFormatting ([url_formatting.md](url_formatting.md)) - VRP: $1,000
    *   Chromium > UI > Browser > TopChrome > SidePanel ([side_panel.md](side_panel.md)?) - VRP: $1,000
    *   Chromium > Blink > WebShare ([webshare.md](webshare.md)) - VRP: $1,000
    *   Chromium > Internals > Plugins > PDF ([plugin_security.md](plugin_security.md)) - VRP: $500
    *   Chromium > Blink > PerformanceAPIs ([performance_apis.md](performance_apis.md)) - VRP: $0
    *   Chromium > Infra > LUCI ([infra_luci.md](infra_luci.md)) - VRP: $0

**Additional Focus Areas (Lower Priority / Cross-Cutting):**

These areas appeared in VRP reports but might be secondary or aspects of broader categories.

*   **Mojo / IPC:** ([ipc.md](ipc.md), [mojo.md](mojo.md)) - Reinforce importance beyond initial listing.
*   **Web Platform APIs (Bluetooth, USB, Serial, Codecs, GPU, XR):** ([bluetooth.md](bluetooth.md), [webusb.md](webusb.md), [webserial.md](webserial.md), [webcodecs.md](webcodecs.md), [webgpu.md](webgpu.md), [webxr.md](webxr.md)) - Already have pages or covered elsewhere.
*   **General Code/Logic & Techniques:** Covered in tips section.
*   **Specific UI Elements (Interstitials, etc.):** Covered in UI Security Tips / existing pages.
*   **Specific APIs (Translate, File System, Push Messaging, Downloads):** Covered in API Tips / existing pages ([translation_ui.md](translation_ui.md)?, [file_system_access.md](file_system_access.md), [push_messaging.md](push_messaging.md), [downloads.md](downloads.md)).
*   **Miscellaneous Side-Channels/Bypasses:** (Canvas, CSS, SVG, WebGL, Copy&Paste, Unicode/IDN, CSRF, SharedWorker, iOS specifics) - These could be integrated into relevant component pages or potentially grouped under a generic "Side Channels" or "Platform Specifics" page if patterns emerge.

By focusing on these VRP-informed areas and techniques, and by continuously updating the wiki, we can improve Chromium security research effectiveness.
