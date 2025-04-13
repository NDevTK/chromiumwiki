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

*   **Focus:** Examine UI rendering logic, especially for security indicators (address bar, permission prompts, dialogs). Look for spoofing, obscuring, bypassing, or clickjacking vulnerabilities. Pay attention to interactions between UI elements (including overlays like PiP, FedCM, extension popups, custom cursors, system notifications), race conditions during state changes (navigation, fullscreen), and edge cases (small windows, RTL text). Test input handling (taps, clicks, keys) when UI elements are partially or fully obscured.
*   **Techniques & VRP Patterns:**
    *   **Address Bar Spoofing:** Analyze omnibox display logic (`AutocompleteInput::Parse`, `url_formatter`), especially on Android. Look for issues with:
        *   *Timing/State:* Slow navigations (VRP: `379652406`, VRP2.txt#2851), scrolling/tab switching races (VRP: `343938078`, VRP2.txt#3274), post-crash/failed navigation state (VRP: `40064170`, `40057561`, VRP2.txt#9502, #7679, #10177), interstitial overwrites (VRP2.txt#14588), downloads (VRP2.txt#14899).
        *   *Scheme Handling:* Delayed/incorrect scheme display (VRP: `40072988`, VRP2.txt#1652), special schemes (`blob:` VRP2.txt#11681, #16811; `javascript:` VRP2.txt#5966, #6008; `data:`). `fido://` deep link issues (VRP2.txt#3016).
        *   *URL Formatting:* Incorrect elision (VRP: `40061104`), long URLs/subdomains (VRP2.txt#5977, #1856, #12072, #8323, #8581), bidirectional text/IDN (VRP2.txt#15139, #12423, #5139, #1618, #15140), Unicode confusables (hyphens VRP2.txt#16501, German Sharp S VRP2.txt#12423, Tibetan Inverted Mchu Can VRP2.txt#16429).
        *   See [url_formatting.md](url_formatting.md), [omnibox.md](omnibox.md), [navigation.md](navigation.md).
    *   **Dialog/Prompt Obscuring:** Test interactions where one UI element can overlay another sensitive one. Check if input is correctly blocked on the obscured element.
        *   *PiP:* Video/Document PiP obscuring Permissions (PEPC) (VRP: `342194497`, VRP2.txt#6928), Autofill (VRP: `40058582`, VRP2.txt#5228), FedCM (VRP: `339654392`, VRP2.txt#12993). File Dialogs (VRP2.txt#14537). `ShowInactive()` usage is often problematic.
        *   *FedCM:* FedCM bubbles obscuring Autofill (VRP: `339481295`, `340893685`, VRP2.txt#7963).
        *   *Extensions:* Extension popups obscuring permissions/screen share (VRP: `40058873`, VRP2.txt#7974), payment requests (VRP2.txt#7574). Inactive extension windows obscuring active prompts/pages (VRP: `40058935`, VRP2.txt#9002, #13551, #12352, #9068).
        *   *System UI/Other:* Share dialogs over browser UI (VRP: `40056848`). System notifications obscuring fullscreen toast (Pop-up blocker VRP2.txt#11735, Download VRP2.txt#10006, #10086, External apps VRP2.txt#12584). Android keyboard/text selection menu obscuring fullscreen toast (VRP2.txt#1873, #13813). `<select>` dropdown obscuring fullscreen toast (VRP2.txt#10560). PWA Install prompt overlays (VRP2.txt#6886). `<select>` options overlaying other tabs (VRP2.txt#7944). Custom CSS cursors overlaying permission prompts (VRP2.txt#15458).
        *   See [fedcm.md](fedcm.md), [picture_in_picture.md](picture_in_picture.md), [permissions.md](permissions.md), [autofill.md](autofill.md), [extensions_api.md](extensions_api.md), [input.md](input.md).
    *   **Dialog/Prompt Spoofing:** Investigate UI origin/content confusion.
        *   *Install Prompts:* PWA install prompts (origin display, overlaying other origins - VRP2.txt#6886, #13834, #7452 - Android; origin hiding via small window VRP2.txt#7537).
        *   *External Protocols:* Dialogs showing wrong origin due to redirects or timing (VRP: `40055515`; VRP2.txt#9087, #13592, #11016, #13605). Dialogs overlapping other origins (VRP2.txt#13555, #13605, #15881). See [protocol_handler.md](protocol_handler.md).
        *   *Chooser Dialogs:* Bluetooth/USB/Serial/HID showing incorrect/missing origin (opaque origin VRP: `40061374`, `40061373`, VRP2.txt#8904, #9771; permission delegation VRP2.txt#10263; Android file picker VRP2.txt#4283).
        *   *Rendering Location:* Prompts rendering outside initiator windows (PEPC - VRP: `341663594`, VRP2.txt#12915; FedCM - VRP: `338233148`, VRP2.txt#13024).
        *   *PDF:* Incorrect X-Frame-Options handling allowing framing (VRP2.txt#11116). See [plugin_security.md](plugin_security.md).
        *   *Download UI:* Origin displayed varies by UI surface (shelf/bubble vs. `chrome://downloads` vs. notifications). `chrome://downloads` uses `FormatUrlForSecurityDisplay`, falling back to literal scheme+path display (e.g., "about:srcdoc") for non-standard schemes (VRP: `916831`, `11062`, VRP2.txt#11028). Long hostnames can still obscure true origin (VRP2.txt#8323, #8581). See [downloads.md](downloads.md).
        *   *Other:* iOS Modal Dialog Spoof (VRP2.txt#3455). `alert()` origin confusion (Incognito VRP2.txt#12741).
        *   See [web_app_identity.md](web_app_identity.md), [navigation.md](navigation.md), [webserial.md](webserial.md), [webusb.md](webusb.md), [bluetooth.md](bluetooth.md), [permissions.md](permissions.md), [fedcm.md](fedcm.md).
    *   **Input/Interaction Hijacking:** Investigate techniques bypassing user intent for interaction.
        *   *Custom Cursors:* Overlaying UI, confusing position (VRP: `40057147` allows 1024px cursor; `1381087`, `1376859`; VRP2.txt#11789, #15458, #13861). Interacting with Permissions prompt (VRP2.txt#11789, #15458). See [input.md](input.md).
        *   *Keyjacking:* Interacting with focused prompts/dialogs while obscured (PaymentRequest - VRP: `1403539`, VRP2.txt#4303; Permissions - VRP: `1371215`, VRP2.txt#1478, #13544; Download UI VRP2.txt#9287). Default-selected accept buttons are risky (PaymentRequest - VRP2.txt#4303). Check `InputEventActivationProtector` usage.
        *   *Tapjacking:* Bypassing interaction delays or visibility checks with rapid/double taps (Permissions - VRP2.txt#1478, #8036, #10088, #15112, #7863; Incognito prompt VRP2.txt#15326). See [permissions.md](permissions.md).
        *   *Focus/Resize Abuse:* Freezing/resizing browser to bypass prompt interaction delays (VRP2.txt#10088, #13545).
        *   *Drag/Drop:* Exploiting drag/drop for interaction/origin bypasses (Portal activation + drag drop SOP bypass VRP2.txt#8707; Drag/drop XSS VRP2.txt#14373). See [drag_and_drop.md](drag_and_drop.md).
        *   *EyeDropper API:* Can confuse cursor position, tricking users (VRP2.txt#12404). See [eye_dropper.md](eye_dropper.md).
        *   See [permissions.md](permissions.md), [payments.md](payments.md), [eye_dropper.md](eye_dropper.md), [input.md](input.md).
    *   **Fullscreen Issues:** Lack of/hiding fullscreen notification (VRP: `1311683`, `1375785`, `1317234` - exit/enter bypass, `1270988` - overlap, `1264561` - cancel bypass; VRP2.txt#7170 - landscape lock bypass, #3726 - landscape lock, #10747 - copy fail toast overlap, #10664 - cancel bypass, #7163 - PiP bypass, #10006/#10086 - download notify overlap, #11735 - popup blocker overlap, #12584 - external app overlap, #1873 - keyboard overlap, #13813 - text select overlap, #10560 - select dropdown overlap, #11602 - general regression). Ability to prevent exit (Esc key blocked by Share API - VRP: `1467734`, Keyboard Lock API - VRP: `1385387`, JS loops - VRP: `1453882`, permission dialog dismissal - VRP2.txt#7701). UI Spoofing in fullscreen (VRP2.txt#7597). See [permissions.md](permissions.md).
    *   **Rendering Issues:** 3D CSS transforms allowing drawing over UI (VRP2.txt#5914, #11463), Video escaping content area (VRP2.txt#10664).

**2. Extension & DevTools Security (High Likelihood, High VRP Value):**

*   **Focus:** Analyze extension API security boundaries, permission models (`<all_urls>`, `debugger`, `downloads`, specific APIs), content script isolation, manifest security (CSP, permissions), and interactions with privileged browser functions or pages (`chrome://`, `devtools://`). Look for sandbox escapes, policy bypasses, unintended API usage, and permission escalation. See [extension_security.md](extension_security.md), [extensions_api.md](extensions_api.md), [extensions_debugger_api.md](extensions_debugger_api.md).
*   **Techniques & VRP Patterns:**
    *   **`chrome.debugger` API Abuse:** This is a major source of high-severity bugs.
        *   *Sandbox Escape via CDP Methods:* Using specific CDP methods via `sendCommand` to bypass checks: `Page.navigate` (to `file://` VRP: `40060173`, VRP2.txt#7661; to privileged URLs), `Page.captureSnapshot`/`Page.captureScreenshot` (local file read VRP2.txt#1116444, #1116445, #3520, #6009, #7621), `Input.synthesizeTapGesture` (VRP2.txt#1178), `Input.dispatchKeyEvent` (VRP2.txt#351), `DOM.setFileInputFiles` (VRP2.txt#15188 - file read), `Page.downloadBehavior` (VRP2.txt#16391 - bypass dangerous download check).
        *   *Target Manipulation:* Attaching to arbitrary/disallowed targets (`Target.attachToTarget` VRP2.txt#16364, `Target.setAutoAttach`+`Target.sendMessageToTarget` VRP2.txt#331). Bypassing permission checks via `targetId` vs `tabId` (VRP: `40056776`).
        *   *Navigation/Timing Races:* Exploiting timing during navigation or crash recovery to attach to or inject script into privileged pages (WebUI, DevTools) (VRP: `41483638`; VRP2.txt#67, #1446, #5705, #1487). Bypassing security interstitials (VRP2.txt#12764).
        *   *Policy Bypass:* Bypassing `runtime_blocked_hosts` (VRP: `40060283`, VRP2.txt#8615, #16467).
    *   **Privileged Page Interaction (Non-Debugger):** Gaining script execution on `chrome://`, `devtools://`, or Web Store pages via other means (e.g., `chrome.devtools.inspectedWindow.eval/reload` VRP: `1473995`, VRP2.txt#12742, #4594, #1059577; WebUI message handler flaws VRP2.txt#2364; DevTools parameter sanitization VRP2.txt#12830, #12809, #15887; Side Panel interaction VRP: `1482786`; custom NTP VRP2.txt#13739). Exploiting `devtools_page` manifest entry (VRP2.txt#5090, #647, #11249). See [devtools.md](devtools.md).
    *   **Permission Escalation/Bypass:** Leaking info or gaining capabilities beyond manifest permissions.
        *   *File Read:* Reading local files with only `downloads` permission (VRP: `1377165`, `1385343`, `1409564`, `989078`; VRP2.txt#1176, #4610, #15974, #1428743, #15940) often via `onDeterminingFilename` or interactions with FSA/`showSaveFilePicker`. Bypasses of file access checks after browser updates (VRP2.txt#5360). Reading via `sourceMappingURL` (`file:` bypass VRP2.txt#5339, `FILE:` bypass VRP2.txt#5278; UNC path leak VRP2.txt#2033, #5314; VRP: `1349146`, `1419604`, `1429241`). Reading via `tabs.captureVisibleTab` (VRP: `810220`, VRP2.txt#1196, #15849).
        *   *Storage Access:* Reading other extension storage (`webview` `loadDataWithBaseUrl` VRP2.txt#11278). Accessing blocked host cookies (VRP: `40060283`; VRP2.txt#13706).
        *   *Tab Info Leak:* Leaking URL/title via `chrome.tabs.onUpdated` without `tabs` permission (VRP: `1306167`).
        *   *Incorrect Update Checks:* Permission errors during extension updates (VRP: `1418104`).
        *   *SOP Bypass:* Malicious extension intercepting import via service worker (VRP2.txt#785).
        *   See [permissions.md](permissions.md), [downloads.md](downloads.md).
    *   **Window Manipulation:** Obscuring active windows (VRP: `40058935`; VRP2.txt#9002, #13551, #12352), moving windows off-screen (VRP: `40058916`; VRP2.txt#9101) via `chrome.windows` API. See [extensions_api.md](extensions_api.md).
    *   **CSP Bypass:** Exploiting manifest CSP validator flaws (`script-src-elem`/`script-src-attr` VRP: `1288035`). See [content_security_policy.md](content_security_policy.md).
    *   **UI Interaction:** Using extension UI (popups) to obscure prompts (VRP: `40058873`, VRP2.txt#7974, VRP2.txt#7574). CSS keylogger via `DeclarativeContent` (VRP2.txt#5763).
    *   **Other:** JavaScript injection in CWS (VRP2.txt#6220).

**3. Autofill & Input Handling Bypasses (High Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate bypasses for autofill security measures (interaction requirements, visibility checks). Analyze input methods, timing attacks, and interactions with other browser features (PiP, FedCM). Look for regressions. See [autofill.md](autofill.md), [input.md](input.md).
*   **Techniques & VRP Patterns:**
    *   **Input Method Abuse:** Bypasses using EyeDropper API (VRP: `40065604`, `40063230`, `40058496`, VRP2.txt#825, #13922), taps (double-taps, rendering near/under cursor - VRP: `40060134`, `40058217`, `40056900`, VRP2.txt#1426679, #1487440, #9878), space key input (VRP: `40056936`), pointer lock (VRP: `40056870`), or physical keyboard accessory (VRP: `VRP2.txt#13367`). Custom CSS cursors (VRP2.txt#13795). See [eye_dropper.md](eye_dropper.md), [pointer_lock.md](pointer_lock.md).
    *   **Visibility Bypass:** Obscuring autofill prompt with PiP windows (VRP: `40058582`, VRP2.txt#5228), FedCM dialogs (VRP: `339481295`, `340893685`, VRP2.txt#7963). Tricking visibility checks by manipulating input field cache/display or using small/clipped windows (VRP: `1395164`, `1358647`, VRP2.txt#1108181, #6717, #3801). See [picture_in_picture.md](picture_in_picture.md), [fedcm.md](fedcm.md).
    *   **Timing/State Issues:** Exploiting delays in prompt rendering after `mousedown` by moving the input field (VRP: `40058217`, `40056900`, VRP2.txt#10877). Exploiting preview state text leaks (`scrollWidth`, font manipulation via `::first-line` or `@font-face` override - VRP: `1035058`, `1035063`, `1013882`, `951487`, `916838`, VRP2.txt#4826, #6190, #6157). Leaking selection via `scrollTop` in `<select>` (VRP: `1250850`). Side-channel via preview (VRP2.txt#14122, #14026).
    *   **Regressions:** Autofill bypasses frequently reappear (VRP: `40063230` regression of `1287364`).

**4. IPC/Mojo Security (Medium Likelihood, High VRP Value - Sandbox Escape Focus):**

*   **Focus:** Deeply analyze IPC messages and Mojo interfaces, especially those callable from less privileged contexts (renderer, GPU process, extensions). Look for insufficient validation (type confusion, integer overflows, missing origin/permission checks), access control flaws, memory safety issues (UAF, buffer overflows), and incorrect state handling (race conditions, dangling pointers). See [ipc.md](ipc.md), [mojo.md](mojo.md).
*   **Techniques & VRP Patterns:**
    *   **Insufficient Validation/Access Control:** Lack of checks on parameters or caller privileges allowed privileged actions (e.g., `StartDragging` from renderer allowing arbitrary mouse control - VRP2.txt#4). Wrong process handling message (Browser handling renderer-intended `ACCEPT_BROKER_CLIENT` - VRP2.txt#370). Lack of origin checks (e.g., `PushMessaging` API - VRP: `1275626`; `ContentIndex` API - VRP: `1263530`, `1263528`; `GetDeveloperIdsTask` VRP2.txt#14587). Incorrect permission checks allowing extensions to send messages to other tabs (VRP2.txt#11815). Insecure handling of `native_struct.mojom`. Unsafe deserialization.
    *   **Privilege Escalation/Sandbox Escapes:** Compromised renderer sending crafted messages to browser process to gain higher privileges or escape sandbox. Exploiting interfaces related to file system access, user input simulation, privileged UI, permissions, DevTools. Exploiting COM interfaces on Windows (`GoogleUpdate.ProcessLauncher` + Session Moniker VRP: `340090047`, VRP2.txt#3763).
    *   **State Confusion/Memory Safety:** Exploiting race conditions or dangling pointers in IPC state management (e.g., `PermissionRequestManager` UAF - VRP: `1424437`). Use-after-free in `observer_list.h` via side panel feature (VRP: `40061678`). Type confusion (VRP: `1337607`). Double fetch leading to state mismatch (VRP2.txt#542 - Code Cache).

**5. Web API & Feature Security (Medium Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate newer or complex web platform features (WebTransport, WebCodecs, WebGPU, WebXR, FedCM, Portals, FencedFrames, File System Access, etc.) and older complex features (Service Workers, CORS, CSP, Sandboxing). Pay attention to their security models, interactions, potential for origin confusion, policy bypass, and side-channel leaks.
*   **Techniques & VRP Patterns:**
    *   **Origin Spoofing/Confusion:** Dialog origin issues (Bluetooth/USB/Serial/HID chooser VRP: `40061374`, `40061373`, VRP2.txt#8904). FedCM prompts with opaque origins (VRP: `340893685`, VRP2.txt#13066). Document PiP origin issues (VRP: `40063068`, `40062959`, `1429246`, `1450728`, VRP2.txt#13133, #14228, #10137, #4398). FencedFrames interaction with PiP (VRP: `40062954`, VRP2.txt#7262). Portal API URL spoof after crash (VRP: `40064170`). External protocol dialogs via redirects (VRP: `40055515`). `javascript:`/`data:` URL origin confusion (VRP: `40059251`, VRP2.txt#173). `blob:` URL origin issues (VRP: `1069246`, `705778`, VRP2.txt#11681, #1761). Navigation not painting content (VRP2.txt#7679). See [fedcm.md](fedcm.md), [picture_in_picture.md](picture_in_picture.md), [fenced_frames.md](fenced_frames.md), [portals.md](portals.md).
    *   **Policy Bypass (CSP, SameSite, Sandbox, Mixed Content, CORS):**
        *   *SameSite:* Bypasses via Web Share API (VRP2.txt#11901), BackgroundFetch (VRP: `1244289`, VRP2.txt#10929, #10871), redirects (VRP: `698492`, VRP2.txt#9752), Service Worker FetchEvent (VRP: `1115438`, VRP2.txt#7521), Android Intents (VRP: `1375132`, VRP2.txt#10199, #5620), Prerender (VRP2.txt#14702, #16021, #14976). Cookie parsing bypass (`=` prepend VRP2.txt#13543). See [privacy.md](privacy.md).
        *   *CSP:* Bypasses via Service Worker intercept (VRP: `598077`), `about:blank` nav (VRP: `996741`, VRP2.txt#1924), `blob:` nav (VRP: `1115628`, VRP2.txt#7831, #11985), `filesystem:` nav (VRP: `1116446`, VRP2.txt#5009), `javascript:` in `srcdoc` (VRP: `1006188`, VRP2.txt#11413), `about:srcdoc` nav (VRP2.txt#11413, #1185), iOS `javascript:` (VRP2.txt#8077), incorrect directive handling (`script-src-elem`/`script-src-attr` VRP: `1288035`, `default-src 'strict-dynamic'` VRP2.txt#11841). CSP reporting leaking fragments (VRP2.txt#15887). See [content_security_policy.md](content_security_policy.md).
        *   *IFrame Sandbox:* Bypasses for `allow-popups-to-escape-sandbox` (VRP: `40069622`, `40057525`, VRP2.txt#7849, #11992, #212). `allow-downloads` bypass (VRP: `40060695`, VRP2.txt#11682). Bypasses via `intent://` URLs (VRP: `1365100`, VRP2.txt#7507, #15706). `javascript:` links with `window.opener` (VRP: `1017879`). Top-level navigation bypasses (`allow-top-navigation` header VRP2.txt#4247, #4910, `onkeydown`/`onblur` VRP2.txt#8121). See [iframe_sandbox.md](iframe_sandbox.md).
        *   *Mixed Content:* Bypass via PWA/ServiceWorker (VRP2.txt#8497), PNA Origin Trial (VRP2.txt#8497). `javascript:` popups (VRP2.txt#9702).
        *   *CORS:* Issues with WebSocket requests (VRP2.txt#1467), general CORS issues (VRP2.txt#15276). Leakage via CORB 'onload' vs 'onerror' (VRP2.txt#13544). BackgroundFetch ACAO wildcard bypass (VRP2.txt#9370). Service Worker incorrectly setting Origin header (VRP2.txt#11875).
    *   **Timing/Side-Channel Leaks:** Subtle information leaks.
        *   *API Leaks:* Performance API (`nextHopProtocol` leak VRP2.txt#13061, redirects VRP: `40054148`, VRP2.txt#14397). History/Navigation leaks (`history.length` VRP: `1208614`, VRP2.txt#5110; `document.baseURI` VRP: `1329654`, VRP2.txt#15203; SoftNav+Paint VRP: `1459093`). `window.length` leak (VRP: `40059056`, VRP2.txt#275).
        *   *Resource Size:* BackgroundFetch leaks (VRP: `1247376`, `1260649`, `1267311`; VRP2.txt#10920, #10690, #10871). Cache API + Range requests (VRP2.txt#14773). `navigator.storage.estimate` padding bypass (VRP2.txt#12007). Range requests + SW + Cache API (VRP2.txt#14773). Range requests + SW + Performance API (VRP2.txt#14397, #14497).
        *   *Pixel/Rendering Timing:* Canvas `drawImage` (VRP: `1093099`, `781017`, `686498`, VRP2.txt#6693). CSS filters/transitions (VRP: `716057`, `1211002`, VRP2.txt#16788). Service Worker + Canvas (VRP2.txt#7318, #7346). WebGL Texture errors leaking redirect URLs (VRP2.txt#16520). SVG filter timing (VRP2.txt#16755). Canvas composite timing (VRP2.txt#16585). WebGL (`getContextAttributes`) reload leak (VRP2.txt#13376). Malicious WebGL page capturing other tabs (VRP2.txt#16788).
        *   *History/Visited Links:* SoftNav+paint (VRP: `1459093`), CSS Paint API (VRP: `680214`), CSS transitions (VRP: `1211002`, VRP2.txt#12845).
        *   *Environment/Other:* Env var leaks via file pickers/downloads (VRP: `1247389`, `1310461`, `1322058`, `1310462`, VRP2.txt#1102, #2919, #2935, #2980). Keystroke timing leaks (VRP: `1315899`, VRP2.txt#4417). See [performance_apis.md](performance_apis.md).
    *   **API Misuse/Interaction:** PaymentRequest API bypasses (VRP: `40072274`, VRP2.txt#4303). Push Messaging API flaws (VRP: `1275626`). Content Index API origin checks (VRP: `1263530`, `1263528`). Scroll inference via Text Fragments (VRP: `1400345`). File System Access API interaction with downloads (VRP: `1428743`). Reading local files via `registerProtocolHandler` (VRP: `971188`). WebRTC port exhaustion (VRP2.txt#6058). Permission Element tapjacking/overlay (VRP2.txt#7863). User Activation abuse (VRP2.txt#9606). Cross-origin fetch leaking objects (VRP2.txt#6873). Service Worker FetchEvent bypassing SameSite (VRP2.txt#7521). Custom Tab scroll inference (VRP2.txt#13156). `fetch.call` cross-origin object leak (VRP2.txt#6873).
*   **Specific APIs:** WebTransport, WebCodecs ([webcodecs.md](webcodecs.md)), WebGPU ([webgpu.md](webgpu.md)), WebXR ([webxr.md](webxr.md)), FedCM ([fedcm.md](fedcm.md)), Web Share API ([webshare.md](webshare.md)), Web Bluetooth ([bluetooth.md](bluetooth.md)), Web USB ([webusb.md](webusb.md)), Web Serial ([webserial.md](webserial.md)), Portals ([portals.md](portals.md)), File System API ([file_system_access.md](file_system_access.md)), Push Messaging API ([push_messaging.md](push_messaging.md)), Translate API ([translation_ui.md](translation_ui.md)), PaymentRequest API ([payments.md](payments.md)), BackgroundFetch API ([background_fetch.md](background_fetch.md)), Content Index API ([content_index.md](content_index.md)), Text Fragments ([text_fragments.md](text_fragments.md)), FencedFrames ([fenced_frames.md](fenced_frames.md)), Service Workers ([service_workers.md](service_workers.md)), SharedWorker ([worker_threads.md](worker_threads.md)), `registerProtocolHandler` ([protocol_handler.md](protocol_handler.md)).

**6. Filesystem & Scheme Handling (Medium Likelihood, Variable VRP Value):**

*   **Focus:** Analyze browser handling of file interactions (`<input type=file>`, drag/drop, File System Access API), special URL schemes (`file://`, `filesystem://`, `content://`, `intent://`, `javascript:`, `data:`, `blob:`, `chrome*://`, `fido://`), and downloads, focusing on security boundaries, information leakage, and policy bypasses. See [file_system_access.md](file_system_access.md), [downloads.md](downloads.md), [intents.md](intents.md).
*   **Techniques & VRP Patterns:**
    *   **Local File Access/Disclosure:**
        *   *Extensions:* Reading files via DevTools debugger interaction (VRP2.txt#1116444, #1116445, #6009, #7621), `downloads` permission + FSA (VRP: `1428743`), `input[type=file]` + symlinks (VRP: `1378484`, `1381634`, VRP2.txt#10231), `sourceMappingURL` (VRP: `1349146`, `1419604`, `1429241`, VRP2.txt#2033, #5278, #5293), `registerProtocolHandler` (VRP: `971188`), `.url` files (VRP: `1303486`), `SharedWorker` (VRP2.txt#670), general `<all_urls>` permission bypasses (VRP2.txt#1216, #5360, #15849).
        *   *Other:* Information disclosure via file picker defaulting (VRP: `756268`). Android private data via intents (VRP: `1198142`). FencedFrames loading local directories (VRP: `1454937`, VRP2.txt#12642). Reading via PDF plugin range requests (VRP2.txt#2403), XSLT external entities (VRP2.txt#8930). CSS Injection (VRP2.txt#16042). `DOM.setFileInputFiles` (VRP2.txt#15188). Chrome Elevation Service LPE (VRP2.txt#6852). Cross-origin read via `fetch` + `reload` + history (VRP2.txt#16438). Drag/Drop leak (VRP2.txt#8707). Reading files from recent folder (VRP2.txt#3479). Reading local files via `window.location` manipulation (VRP2.txt#15110).
    *   **Special Scheme Handling:** Bypasses/exploits involving `intent://` (Redirects, Sandbox bypass VRP: `40064598`, `1365100`, `1375132`; VRP2.txt#7507, #10199, #15724, #8642). `android-app://` (VRP: `1092025`, VRP2.txt#8165). `javascript:` (Origin confusion VRP: `40059251`, VRP2.txt#173; CSP bypass VRP: `1006188`, VRP2.txt#8077; Self-XSS VRP: `850824`, `738694`, VRP2.txt#5966, #6008; SOP bypass VRP2.txt#11380; Document PiP spoof VRP2.txt#15301). `file://` (Navigation bypasses VRP: `40060173`; Reading via extensions, see above; UNC paths in `sourceMappingURL` VRP: `1342722`, `1379885`, VRP2.txt#2093, #5314; Symlink traversal VRP2.txt#10231; Drag-and-drop filter bypass leading to RCE via allowed handler VRP2.txt#11682). `chrome-untrusted://` (XSS VRP: `40057777`; Interaction VRP2.txt#8663). `fido://` (Deep link hijacking VRP2.txt#3016). `content://` (Bypass VRP2.txt#1865, #8165). `chrome-devtools://` (Reading C: drive VRP2.txt#12169). `chrome-error://` (Interstitial bypass VRP2.txt#12764). `tel:` spoof (VRP2.txt#14950, #14963, #15960). `blob:` (SOP/Site Iso bypass VRP2.txt#1761, #679; CSP bypass VRP2.txt#7831; Origin display VRP2.txt#16811). `filesystem:` (CSP bypass VRP2.txt#5009; Site Iso bypass VRP2.txt#6261).
    *   **Download Security:**
        *   *Origin Spoofing:* Misleading origin display in various UI surfaces (see UI section).
        *   *File Type Masking:* Using double extensions (`.exe.txt`), special characters (`%%`), or Save As dialog behavior to bypass extension checks. `net::GetMimeTypeFromFile` relies on last extension. Native dialogs may prioritize user input. (VRP: `1228653`, `1378895`, `1303486`; VRP2.txt#14019, #15866).
        *   *Safe Browsing Bypass:* Using large data URIs/long referrer chains to cause SB ping failure (HTTP 413), resulting in `UNKNOWN` verdict treated as `SAFE` (VRP: `1416794`, VRP2.txt#10091, #10114). Bypass via `embed` tag (VRP2.txt#6559).
        *   *OS Handler Abuse:* Opening downloaded files (like `.SettingContent-ms`, `.lnk`, `.url`) via `platform_util::OpenItem` -> `ShellExecuteExW` can lead to code execution if the user bypasses SB warnings (VRP: `40060695`). Safe Browsing *does* classify these extensions, but the execution happens via OS delegation.
        *   *Silent Downloads:* Bypassing user confirmation via `showSaveFilePicker` (VRP: `1243802`, VRP2.txt#9302), Web Share API (VRP2.txt#5730).
        *   *Other:* History manipulation (VRP: `1513412`, VRP2.txt#3449), DevTools bypass (VRP2.txt#16391).
        *   See [safe_browsing_service.md](safe_browsing_service.md).

**7. Installer/Updater Security (Windows/macOS) (Medium Likelihood, High VRP Value - EoP Focus):**

*   **Focus:** Analyze installer (`setup.exe`, `.msi`, `.pkg`) and updater components (Google Update Service, Keystone) for Elevation of Privilege (EoP) vulnerabilities. Focus on file/registry operations in temp/shared locations, IPC/COM/XPC interfaces, and signature/version check logic. See [installer_security.md](installer_security.md).
*   **Techniques & VRP Patterns:**
    *   **Insecure File Operations:** Arbitrary file create/write/delete in user-writable or predictable locations (`C:\Windows\Temp`, `%APPDATA%\Local`, `C:\ProgramData\Google\Update\Log`, `/tmp/`) via symlinks/hardlinks/junctions during install/update/uninstall/logging/crashpad. (VRP: `1317661`, `1183137`, `704138`; VRP2.txt#1259, #4151, #9914, #11914, #12831, #12876, #1390, #4207, #1333, #1362, #12831, #12876 - installer/updater/crashpad/uninstaller paths).
    *   **Race Conditions:** TOCTOU during file caching, version checks vs. signature checks, log rotation. (VRP: `1152971`, VRP2.txt#157 - update cache; VRP2.txt#914 - Keystone version vs codesign; VRP2.txt#1191 - logging).
    *   **Insecure Permissions:** Incorrect ACLs set on created files/dirs (VRP2.txt#4151). Incorrect handling of registry symlinks during deletion by uninstaller (VRP2.txt#8739 - `Active Setup`). Exploiting `AlwaysInstallElevated` policy (VRP2.txt#8739).
    *   **Insecure IPC/COM/XPC (EoP):** Exposed COM objects (`GoogleUpdate.ProcessLauncher`) allowing actions in other sessions via Session Moniker (VRP: `340090047`, VRP2.txt#3763). Arbitrary file read via COM object (`IAppBundleWeb->download`) due to missing impersonation (`CallAsSelfAndImpersonate2`) (VRP2.txt#1152). Arbitrary file read via `IGoogleUpdate3Web` (VRP2.txt#6482).
    *   **Signature/Integrity Bypass:** Lack of code signing checks (VRP2.txt#914 - Keystone install). CRX3 signature verification bypass via embedded ZIP64 (VRP2.txt#3063).

**8. General Code Analysis Techniques:**

*   **Fuzzing:** Target APIs, file parsers (PDF VRP: `40059101`), IPC/Mojo interfaces, complex C++ codecs (WebCodecs) with fuzzers like libFuzzer. Focus on edge cases, state transitions, and complex inputs.
*   **Manual Code Review:** Focus on security boundaries: browser↔renderer IPC/Mojo, process isolation logic (`SiteInstance`, RPH), permission checks (API implementations, `ChildProcessSecurityPolicy`), input validation (URL parsing, UI event handling), scheme handling, privilege management (installers/updaters). Look for common C++ vulns (TOCTOU, overflows, UAF - VRP: `40061678`, type confusion - VRP: `1337607`), logic flaws, insecure assumptions, error handling gaps, resource management issues.
*   **Variant Analysis:** Use findings (especially from VRPs) to search for similar patterns (CodeQL, grep) across the codebase. Check for incomplete fixes.
*   **Exploit Technique Awareness:** Understand common browser exploitation techniques (e.g., UAF, type confusion, JIT spraying) and OS-level EoP methods (Windows symlink/junction/oplock abuse, COM interface issues, macOS XPC/Keystone issues) to guide analysis.
*   **Corner Cases & Regressions:** Test edge cases (malformed inputs, race conditions, error states, feature interactions). Verify fixes, especially in complex areas (Autofill, Permissions, Navigation).
*   **Platform & Context Specificity:** Test across platforms (Win, Mac, Linux, Android, CrOS, iOS), checking platform APIs (Intents, XPC) and UI paradigms. Test specifically in contexts like WebView, Extensions, PWAs, Incognito.

**9. Data Transfer Boundaries & Trust (Medium-High Likelihood, Variable VRP Value):**

*   **Focus:** Analyze features involving multi-step data transfer across process or privilege boundaries (Drag and Drop, Copy/Paste, File System Access). Pay close attention to where data originates, how it's packaged, where trust decisions are made, and when permissions are granted. Look for opportunities to spoof origin/taint information, bypass intermediate checks, or exploit timing windows between checks and actions. See [drag_and_drop.md](drag_and_drop.md), [ipc.md](ipc.md), [file_system_access.md](file_system_access.md).
*   **Techniques & VRP Patterns:**
    *   **Origin/Taint Spoofing:** Can a less privileged context (renderer, guest VM) make data appear to originate from a more trusted source (browser UI, OS)? Investigate how source information (e.g., `IsRendererTainted` flag in `ui::OSExchangeData`) is set and propagated. Check if intermediate components (e.g., Exo delegates like `ChromeDataExchangeDelegate`) correctly preserve or verify source context before passing data along.
    *   **Permission Granting Timing:** Analyze *when* permissions are granted relative to *when* data is validated or acted upon. Example: In drag-and-drop, file permissions (`GrantReadFile`, `GrantReadFileSystem` via `PrepareDropDataForChildProcess`) are granted based on `DropData` *just before* the `DragTargetDrop` IPC. Could `DropData` be tampered with between initial UI processing and this final permission grant? Look for similar patterns in FSA or clipboard operations where data might be modified after initial checks but before final access grants.
    *   **Delegate/Policy Bypass:** Examine intermediate delegates or policy checks (e.g., `HandleOnPerformingDrop` for enterprise scanning, `DataExchangeDelegate` for Exo context) that might filter or modify data. Can these checks be bypassed? Do they make incorrect assumptions based on potentially spoofed origin/taint info? Does filtering happen *before* potentially risky actions (like granting file access based on initial paths)?
    *   **Data Format/Parsing:** Investigate parsing of complex data formats transferred across boundaries (e.g., pickled `fs/sources` data parsed by `file_manager::util::ParseFileSystemSources`). Ensure robust validation against malformed or unexpected input, especially if parsing logic differs based on the (potentially spoofed) source endpoint.
    *   **Context Loss:** Can the original security context (e.g., cross-site initiator) be lost during complex flows, leading to incorrect policy application later (e.g., SameSite cookie bypass via `DownloadURL` drag - VRP: `40060358`, VRP2.txt#283)? Trace initiator information through asynchronous steps or handoffs to different components (e.g., `DragDownloadFile` -> `DownloadManager`).
    *   **Copy/Paste XSS:** Exploiting differences in HTML parsing/sanitization between clipboard writing and reading (VRP2.txt#900, #4043, #1065761, #1141350, #14643, #10912, #14824). Focus on foreign content (`<math>`, `<svg>`), templates, and tags with special parsing modes (`<noscript>`, `<style>`, `<textarea>`, `<xmp>`, `<plaintext>`). `navigator.clipboard.read()` sanitization bypass (VRP2.txt#10581).

**10. Parsing & Content Handling (Variable Likelihood/Impact):**

*   **Focus:** Analyze how Chromium parses and handles various content types (HTML, CSS, JS, XML, SVG, media files, fonts, etc.). Look for parsing differentials (mXSS), incorrect state transitions, type confusion, mishandling of malformed data, and insecure interactions between parsers and other components (e.g., script execution, style application).
*   **Techniques & VRP Patterns:**
    *   **Parsing Differentials (mXSS):** Exploiting differences between parsers (e.g., main HTML parser vs. `innerHTML`, `DOMParser`, `document.write`, clipboard sanitizer parser, `tree_builder_simulator` for `srcdoc` - VRP2.txt#14643). Look for inconsistencies in handling foreign content (`<math>`, `<svg>`), templates, `noscript`, and tags with special text modes (`<style>`, `<textarea>`, `<xmp>`, `<plaintext>`).
    *   **Scheme/Type Handling:** Incorrect handling based on URL scheme or content type.
        *   iOS rendering XML/SVG despite `text/plain` (VRP2.txt#8287).
        *   Liberal CSS parsing allowing injection from `file:///` resources (VRP2.txt#16042).
    *   **Resource Leaks via Parsing:** Information leaks triggered during parsing/handling.
        *   PDF page count leak via `getThumbnail()` CHECK (VRP: `40059101`).
        *   JS Error stack traces leaking redirect URLs (VRP2.txt#5947).
        *   CSP reporting leaking URL fragments (VRP2.txt#15887).

**Wiki Pages (Prioritized List - *Adjust Dynamically Based on Research*):**

This list prioritizes components based on a combination of VRP value and frequency. Links point to existing or newly created pages.

*   **High VRP / High Frequency:**
    *   Chromium > Internals > Sandbox > SiteIsolation ([site_isolation.md](site_isolation.md)) - VRP: $20,000
    *   Chromium > Installer & Updater (Windows/macOS) ([installer_security.md](installer_security.md)) - VRP: $20,000+
    *   Chromium > Mobile > WebView ([android_webview.md](android_webview.md)) - VRP: $15,000
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
    *   Chromium > Platform > Extensions > API ([extensions_api.md](extensions_api.md)) - VRP: $3,000
    *   Chromium > Mobile > Intents ([intents.md](intents.md)) - VRP: $3,000
    *   Chromium > Blink > Input > PointerLock ([pointer_lock.md](pointer_lock.md)) - VRP: $3,000
    *   Chromium > Blink > Serial ([webserial.md](webserial.md)) - VRP: $3,000
    *   Chromium > Privacy ([privacy.md](privacy.md)) - VRP: $3,000
    *   Chromium > Blink > USB ([webusb.md](webusb.md)) - VRP: $3,000
    *   Chromium > Blink > SecurityFeature > COOP ([coop.md](coop.md)) - VRP: $3,000
    *   Chromium > Blink > Bluetooth ([bluetooth.md](bluetooth.md)) - VRP: $3,000
    *   Chromium > Blink > Forms > Color ([input.md](input.md)?) - VRP: $2,000 (Related to Input/Autofill)
    *   Chromium > UI > Browser > Navigation ([navigation.md](navigation.md)) - VRP: $2,000
    *   Chromium > Blink > Input ([input.md](input.md)) - VRP: $2,000
    *   Chromium > Blink > Portals ([portals.md](portals.md)) - VRP: $2,000
    *   Chromium > UI > Browser > SafeBrowsing ([safe_browsing_service.md](safe_browsing_service.md)) - VRP: $2,000
    *   Chromium > Blink > ServiceWorkers ([service_workers.md](service_workers.md)) - VRP: ~$2k (Based on related bypasses)
    *   Chromium > Internals > Security > DevTools ([devtools.md](devtools.md)) - VRP: Variable (High via Debugger)
    *   Chromium > Internals > GPU Process ([gpu_process.md](gpu_process.md)) - VRP: $0 (High potential for SBX/MemSafety)
*   **Lower VRP / Lower Frequency / Emerging Tech:**
    *   Chromium > Blink > DataTransfer ([drag_and_drop.md](drag_and_drop.md)) - VRP: $1,000
    *   Chromium > UI > Security > UrlFormatting ([url_formatting.md](url_formatting.md)) - VRP: $1,000
    *   Chromium > UI > Browser > TopChrome > SidePanel ([side_panel.md](side_panel.md)) - VRP: $1,000
    *   Chromium > Blink > WebShare ([webshare.md](webshare.md)) - VRP: $1,000
    *   Chromium > Internals > Plugins > PDF ([plugin_security.md](plugin_security.md)) - VRP: $500
    *   Chromium > Blink > PerformanceAPIs ([performance_apis.md](performance_apis.md)) - VRP: $0
    *   Chromium > Infra > LUCI ([infra_luci.md](infra_luci.md)) - VRP: $0
    *   Chromium > Blink > Media > WebCodecs ([webcodecs.md](webcodecs.md)) - VRP: $0 (Likely High potential)
    *   Chromium > Blink > WebGPU ([webgpu.md](webgpu.md)) - VRP: $0 (Likely High potential)
    *   Chromium > Blink > WebXR ([webxr.md](webxr.md)) - VRP: $0 (Likely Medium potential)
    *   Chromium > Blink > BackgroundFetch ([background_fetch.md](background_fetch.md)) - VRP: $0 (Related to $3k SameSite)
    *   Chromium > Blink > PushMessaging ([push_messaging.md](push_messaging.md)) - VRP: $0 (Related to $1k IPC bypass)
    *   Chromium > Blink > SecurityFeature > FencedFrames ([fenced_frames.md](fenced_frames.md)) - VRP: $0 (Related to $2k PiP / File access)
    *   Chromium > Mojo IPC Framework ([mojo.md](mojo.md)) - VRP: Variable (High potential)
    *   Chromium > Blink > Workers > Shared Workers ([worker_threads.md](worker_threads.md)) - VRP: $0 (Related to file read VRP)
    *   Chromium > UI > Browser > Translate ([translation_ui.md](translation_ui.md)) - VRP: $0 (General UI patterns apply)
    *   Chromium > Blink > Navigation > Text Fragments ([text_fragments.md](text_fragments.md)) - VRP: $0 (Related to $1.4k side channel)
    *   Chromium > Blink > Content Index API ([content_index.md](content_index.md)) - VRP: $0 (Related to $1k IPC bypass)
    *   Chromium > Blink > Custom Handlers > Protocol Handler ([protocol_handler.md](protocol_handler.md)) - VRP: $0 (Related to $9k+ file read)

By focusing on these VRP-informed areas and techniques, and by continuously updating the wiki, we can improve Chromium security research effectiveness.