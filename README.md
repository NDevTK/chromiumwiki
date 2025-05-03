# Chromium Security Wiki

This wiki contains analysis of Chromium components and potential security vulnerabilities.

**Important Notes & Guiding Principles:**

*   **Dynamic Prioritization:** Research focus and wiki page order are dynamic. While initial prioritization considers bug likelihood and VRP reward, **continuously adjust based on new findings, research progress, emerging vulnerability types, and user guidance.**
    *   Codebase analysis revealing promising patterns increases priority.
    *   Lack of progress in an area may decrease priority, while significant findings increase it.
    *   New vulnerability intelligence should lead to reprioritization of related areas.
*   **Continuous Updates:** Integrate all research findings, analysis, potential issues, and relevant VRP examples *directly* into the relevant wiki page following the defined format. **Avoid separate notes files.** Promptly update wiki pages to reflect the latest understanding as new ideas, findings, or VRP data from `VRP.txt` and `VRP2.txt` emerge.
*   **Detail is Key:** The "General Security Research Tips" section must remain highly detailed, drawing heavily on examples from the VRP data to provide actionable guidance.

**Format of Each Wiki Page:**

Each wiki page follows a consistent format:

1.  **Component Focus:** Clearly state the specific Chromium component(s) being analyzed (e.g., `content/browser/renderer_host/render_widget_host_view_aura.cc`, `components/autofill/content/renderer/autofill_agent.cc`).
2.  **Potential Logic Flaws & VRP Relevance:** List potential logic flaws or vulnerabilities. Briefly describe each issue, its potential impact, and cite relevant VRP examples if applicable (e.g., "Origin confusion in DevTools - See VRP #12345").
3.  **Further Analysis and Potential Issues:** Provide a more detailed analysis of identified issues, highlighting specific areas of concern within the codebase. Integrate research notes (files reviewed, key functions). Summarize relevant CVEs/VRP reports and their connection to the discussed functionalities.
4.  **Code Analysis:** Include specific code snippets and observations relevant to potential vulnerabilities.
5.  **Areas Requiring Further Investigation:** List specific questions, functions, or interactions needing deeper analysis.
6.  **Related VRP Reports:** Link to specific VRP reports (like those in `VRP.txt` and `VRP2.txt`) that demonstrate vulnerabilities in this or related components.
7.  **Cross-References:** Link to other related wiki pages.

**General Security Research Tips:**

This section provides actionable tips for security research in Chromium, informed by common vulnerability patterns observed in VRP reports.

**1. UI Security: Spoofing & Obscuring (High Likelihood, High VRP Value):**

*   **Focus:** Examine UI rendering logic, especially for security indicators (address bar, permission prompts, dialogs). Look for spoofing, obscuring, bypassing, or clickjacking vulnerabilities. Pay attention to interactions between UI elements (including always-on-top overlays like **PiP**, FedCM bubbles, extension popups, custom cursors, system notifications), race conditions during state changes (navigation, fullscreen), and edge cases (small windows, RTL text). Test input handling (taps, clicks, keys) when UI elements are partially or fully obscured, noting that UI shown via `ShowInactive()` (like Video PiP or certain prompts) may not prevent keyboard interaction with underlying elements. Check for missing `InputEventActivationProtector` on sensitive UI.
*   **Techniques & VRP Patterns:**
    *   **Address Bar Spoofing:** Analyze omnibox display logic (`AutocompleteInput::Parse`, `url_formatter`). Look for:
        *   *Timing/State:* Slow navigations (VRP: `379652406`, VRP2.txt#2851), scrolling/tab switching races (VRP: `343938078`, VRP2.txt#3274), post-crash/failed navigation state (VRP: `40064170` - Portals, `40057561` - Site Isolation, VRP2.txt#9502, #7679, #10177), interstitial overwrites (VRP2.txt#14588), download interactions (VRP2.txt#14899).
        *   *Scheme Handling:* Delayed/incorrect scheme display (VRP: `40072988` - Android, VRP2.txt#1652), special schemes (`blob:` VRP2.txt#11681, #16811; `javascript:` VRP2.txt#5966, #6008; `data:`). `fido://` issues (VRP2.txt#3016).
        *   *URL Formatting/Elision:* Incorrect elision (VRP: `40061104` - Web Share), long URLs/subdomains hiding origin (VRP2.txt#5977, #1856, #12072, #8323, #8581), Bidi/IDN/Confusables (VRP2.txt#15139, #12423, #5139, #1618, #15140, #16501, #16429).
        *   See [url_formatting.md](url_formatting.md), [omnibox.md](omnibox.md), [navigation.md](navigation.md).
    *   **Dialog/Prompt Obscuring:** Test UI element overlays. Check if input is correctly blocked (beware `ShowInactive()` effects).
        *   *PiP:* Video/Document PiP obscuring Permissions (PEPC) (VRP: `342194497`, VRP2.txt#6928), Autofill (VRP: `40058582`, VRP2.txt#5228), FedCM (VRP: `339654392`, VRP2.txt#12993), File Dialogs (VRP2.txt#14537).
        *   *FedCM:* FedCM bubbles obscuring Autofill (VRP: `339481295`, `340893685` - opaque origin, VRP2.txt#7963).
        *   *Extensions:* Extension popups obscuring Permissions/Screen Share (VRP: `40058873`, VRP2.txt#7974), Payment Requests (VRP2.txt#7574). Inactive extension windows over active content (VRP: `40058935`, VRP2.txt#9002, #13551, #12352, #9068).
        *   *System UI/Other:* Share dialogs (VRP: `40056848`). System notifications over fullscreen toast (VRP2.txt#11735, #10006, #10086, #12584). Android Keyboard/Text Select over fullscreen toast (VRP2.txt#1873, #13813). `<select>` dropdown over toast/other tabs (VRP2.txt#10560, #7944). Custom CSS cursors over prompts (VRP2.txt#15458). PWA Install overlays (VRP2.txt#6886).
        *   See [fedcm.md](fedcm.md), [picture_in_picture.md](picture_in_picture.md), [permissions.md](permissions.md), [autofill.md](autofill.md), [extension_security.md](extension_security.md), [input.md](input.md).
    *   **Dialog/Prompt Spoofing (Origin/Content):**
        *   *Install Prompts:* PWA install origin display/overlay/hiding issues (VRP2.txt#6886, #13834, #7452, #7537). Android app install intent spoof (VRP: `41493134`).
        *   *External Protocols:* Wrong origin via redirects/timing (VRP: `40055515`; VRP2.txt#9087, #13592, #11016, #13605). Overlapping other origins (VRP2.txt#13555, #13605, #15881). See [protocol_handler.md](protocol_handler.md).
        *   *Chooser Dialogs:* Incorrect/missing origin for Bluetooth/USB/Serial/HID (opaque origins VRP: `40061374` - Serial, `40061373` - Android BT/USB; VRP2.txt#8904, #9771; permission delegation VRP2.txt#10263; Android file picker VRP2.txt#4283). See [webusb.md](webusb.md), [bluetooth.md](bluetooth.md).
        *   *Rendering Location:* Prompts outside initiator window (PEPC VRP: `341663594`, VRP2.txt#12915; FedCM VRP: `338233148`, VRP2.txt#13024).
        *   *PDF:* X-Frame-Options bypass (VRP2.txt#11116). See [plugin_security.md](plugin_security.md).
        *   *Download UI:* Inconsistent origin display (VRP: `916831`, `11062`, VRP2.txt#11028). See [downloads.md](downloads.md).
        *   *Other:* iOS Modal Spoof (VRP2.txt#3455). `alert()` Incognito origin (VRP2.txt#12741). Web Share dialog URL elision (VRP: `40061104`).
        *   See [web_app_identity.md](web_app_identity.md), [navigation.md](navigation.md).
    *   **Context Confusion in UI Display:** **Verify that UI elements displaying security-sensitive information (like URLs/origins) derive that information from the correct context.** For features involving multiple `WebContents` (e.g., Document PiP with parent/child, Portals), ensure the UI reflects the context of the *displayed content* (child) and not the *initiating context* (parent). Failure to do so leads to origin spoofing (e.g., Document PiP frame showing parent origin - VRP: `40063068`, VRP2.txt#10137; Fenced frame spoofing Document PiP - VRP: `40062954`, VRP2.txt#7262). Check functions like `GetWebContents()` vs. `GetChildWebContents()` usage in UI code. See [picture_in_picture.md](picture_in_picture.md), [portals.md](portals.md), [fenced_frames.md](fenced_frames.md).
    *   **Input/Interaction Hijacking:**
        *   *Custom Cursors:* Overlaying UI, confusing position (VRP: `40057147` - 1024px via compromised renderer; VRP2.txt#11789, #15458, #13861). Interacting with Permissions prompt (VRP2.txt#11789, #15458). See [input.md](input.md).
        *   *Keyjacking:* Interacting with obscured focused prompts (PaymentRequest VRP2.txt#4303; Permissions VRP2.txt#1478, #13544; Downloads VRP2.txt#9287). Extension inactive window (VRP: `40058935`). Document PiP resize/move (VRP: `40063071`). Risky with default 'accept' buttons. Check `InputEventActivationProtector` usage on target UI. Note the inconsistent use: while FedCM, Downloads, and Payments UI employ this protector, core Permissions and Autofill UI currently do not, making them prime targets for interaction bypasses.
        *   *Tapjacking:* Bypassing interaction delays with rapid/double taps (Permissions VRP2.txt#1478, #8036, #10088, #15112, #7863; Incognito VRP2.txt#15326). Autofill bypass (VRP: `40060134`, `40065604` - EyeDropper). See [permissions.md](permissions.md), [autofill.md](autofill.md).
        *   *Focus/Resize Abuse:* Bypassing delays via window manipulation (VRP2.txt#10088, #13545). Extension moving window offscreen (VRP: `40058916`). Document PiP resize/move (VRP: `40063071`).
        *   *Drag/Drop:* Interaction/origin bypasses (Portal + SOP VRP2.txt#8707; XSS VRP2.txt#14373). See [drag_and_drop.md](drag_and_drop.md).
        *   *EyeDropper API:* Confusing cursor position (VRP2.txt#12404). Used for autofill bypass (VRP: `40065604`, `40058496`, `40063230`). See [eye_dropper.md](eye_dropper.md).
        *   See [payments.md](payments.md).
    *   **Fullscreen Issues:** Hiding/obscuring notification (VRP: `1311683`, `1375785`, `1317234`; VRP2.txt#7170, #3726, #10747, #10664, #7163, #10006, #10086, #11735, #12584, #1873, #13813, #10560, #11602). Preventing exit (VRP: `1467734`, `1385387`, `1453882`; VRP2.txt#7701). UI Spoofing (VRP2.txt#7597). See [permissions.md](permissions.md).
    *   **Rendering Issues:** 3D CSS transforms over UI (VRP2.txt#5914, #11463). Video escaping bounds (VRP2.txt#10664).
    *   **Memory Safety:** Heap UAF in Side Panel via Notes/Annotation (VRP: `40061678`).

**2. Extension & DevTools Security (High Likelihood, High VRP Value):**

*   **Focus:** Analyze extension API boundaries, permissions (`<all_urls>`, `debugger`, `downloads`), content script isolation, manifest CSP, interactions with privileged pages (`chrome://`, `devtools://`). Look for sandbox escapes, policy bypasses, unauthorized API use, permission escalation. See [extension_security.md](extension_security.md), [extensions_debugger_api.md](extensions_debugger_api.md), [devtools.md](devtools.md).
*   **Techniques & VRP Patterns:**
    *   **`chrome.debugger` API Abuse:** Major source of high-severity bugs due to potential to bypass checks in backend CDP handlers.
        *   *CDP Method Exploits:* `Page.navigate` (`file:` VRP: `40060173`, VRP2.txt#7661), `Page.captureSnapshot`/`captureScreenshot` (file read VRP2.txt#1116444, #3520), `Input.synthesizeTapGesture` (VRP2.txt#1178), `Input.dispatchKeyEvent` (VRP2.txt#351), `DOM.setFileInputFiles` (file read VRP2.txt#15188), `Page.downloadBehavior` (download bypass VRP2.txt#16391).
        *   *Target Manipulation:* Attaching to disallowed targets (`Target.attachToTarget` VRP2.txt#16364, `setAutoAttach` VRP2.txt#331). `targetId` vs `tabId` confusion allowing access to incognito/other profiles (VRP: `40056776`).
        *   *Timing Races:* Attaching to privileged pages during navigation/crash (VRP: `41483638`; VRP2.txt#67, #1446, #5705, #1487). Interstitial bypass (VRP2.txt#12764).
        *   *Policy Bypass:* `runtime_blocked_hosts` (VRP: `40060283`, VRP2.txt#8615, #16467, #13706).
    *   **Privileged Page Interaction (Non-Debugger):** Script execution via `devtools_page` manifest entry (VRP2.txt#5090, #647, #11249), `inspectedWindow.eval/reload` (VRP: `1473995`, VRP2.txt#12742, #4594), WebUI flaws (VRP2.txt#2364), DevTools parameter sanitization (VRP2.txt#12830, #12809, #15887), Side Panel (VRP: `1482786`), Custom NTP (VRP2.txt#13739). XSS via `chrome-untrusted://new-tab-page` (VRP: `40057777`).
    *   **Permission Escalation/Bypass:**
        *   *File Read:* Via `downloads` permission + FSA (VRP: `1428743`) or `onDeterminingFilename` (VRP: `1377165`, VRP2.txt#1176, #4610, #15974, #14189). Via `sourceMappingURL` (`file:`/UNC VRP: `1349146`, VRP2.txt#2033, #5278, #5314, #5339). Via `tabs.captureVisibleTab` (VRP: `810220`, VRP2.txt#1196, #15849). Bypass file access checks post-update (VRP2.txt#5360).
        *   *Storage Access:* Other extension storage (`webview` VRP2.txt#11278). Blocked host cookies (VRP: `40060283`; VRP2.txt#13706).
        *   *Tab Info Leak:* `tabs.onUpdated` without `tabs` perm (VRP: `1306167`). Debugger listing URLs across profiles (VRP: `40056776`).
        *   *SOP Bypass:* Service worker intercept (VRP2.txt#785).
        *   See [permissions.md](permissions.md), [downloads.md](downloads.md), [file_system_access.md](file_system_access.md).
    *   **Window Manipulation:** Obscuring/moving windows via `chrome.windows` (VRP: `40058935` - inactive over active, `40058916` - move offscreen; VRP2.txt#9002, #9101).
    *   **CSP Bypass:** Manifest validator flaws (`script-src-elem` VRP: `1288035`). See [content_security_policy.md](content_security_policy.md).
    *   **UI Interaction:** Popups obscuring prompts (VRP: `40058873`, VRP2.txt#7974, #7574). CSS keylogger (`DeclarativeContent` VRP2.txt#5763).

**3. Autofill & Input Handling Bypasses (High Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate bypasses for autofill interaction/visibility checks. Analyze input methods (EyeDropper, taps, PointerLock), timing attacks, interactions with overlays (PiP, FedCM). See [autofill.md](autofill.md), [input.md](input.md).
*   **Techniques & VRP Patterns:**
    *   **Input Method Abuse:** EyeDropper (VRP: `40065604` - 2 taps, `40058496`, `40063230` - regression; VRP2.txt#825), Taps (VRP: `40060134` - 2 taps, VRP2.txt#9878), Space Key (VRP: `40056936`), Pointer Lock (VRP: `40056870`), Keyboard Accessory (VRP2.txt#13367), Custom Cursor (VRP2.txt#13795). See [eye_dropper.md](eye_dropper.md), [pointer_lock.md](pointer_lock.md).
    *   **Visibility Bypass:** PiP obscuring (VRP: `40058582`, VRP2.txt#5228), FedCM obscuring (VRP: `339481295`, VRP2.txt#7963). Tricking checks via small windows/clipping (VRP: `1395164`, VRP2.txt#6717, #3801). Rendering prompt under cursor (VRP: `40056900` - via page, `40056936` - via space key).
    *   **Timing/State Issues:** Moving input after `mousedown` (VRP: `40058217`, VRP2.txt#10877). Preview text leaks (`scrollWidth`, CSS VRP: `1035058`, VRP2.txt#4826). `scrollTop` leak (VRP: `1250850`). Side-channels (VRP2.txt#14122).
    *   **Regressions:** Common (VRP: `40063230`).

**4. IPC/Mojo Security (Medium Likelihood, High VRP Value - Sandbox Escape Focus):**

*   **Focus:** Analyze Mojo interfaces crossing privilege boundaries (Renderer->Browser). Look for insufficient validation (origin/permission checks, parameters), UAF/memory safety, state confusion. See [ipc.md](ipc.md), [mojo.md](mojo.md).
*   **Techniques & VRP Patterns:**
    *   **Insufficient Validation:** Missing origin checks (PushMessaging VRP: `1275626`; ContentIndex VRP: `1263530`; BackgroundFetch VRP2.txt#14587; `javascript:`/`data:` URL confusion VRP: `40059251`). Missing browser-side capability checks (FencedFrames VRP2.txt#225). Incorrect message routing (VRP2.txt#370). Privileged actions from renderer (`StartDragging` VRP2.txt#4). WebUI MojoJS bindings enabled for subsequent site (VRP: `40053875`).
    *   **Privilege Escalation/SBX:** Renderer -> Browser message exploits. Windows COM EoP (`GoogleUpdate.ProcessLauncher` Session Moniker VRP2.txt#3763).
    *   **State Confusion/Memory Safety:** UAF (`PermissionRequestManager` VRP: `1424437`; Side Panel VRP: `40061678`). Type confusion (VRP: `1337607`). Double fetch (Code Cache VRP2.txt#542). Information leak (`window.length` VRP: `40059056`). Site Isolation URL Spoof after crash (VRP: `40057561`).

**5. Web API & Feature Security (Medium Likelihood, Medium-High VRP Value):**

*   **Focus:** Investigate newer/complex APIs (WebTransport, WebCodecs, WebGPU, WebXR, FedCM, Portals, FencedFrames, FSA) and core features (Service Workers, CORS, CSP, Sandbox). Look for origin confusion, policy bypasses, side-channels.
*   **Techniques & VRP Patterns:**
    *   **Origin Spoofing/Confusion:** Dialogs (Device Choosers VRP: `40061374`, VRP2.txt#8904; FedCM VRP2.txt#13066; Document PiP VRP: `40063068`, VRP2.txt#10137; FencedFrames+PiP VRP: `40062954`, VRP2.txt#7262; Portals VRP: `40064170`; External Protocol VRP: `40055515`). Special schemes (`javascript:` VRP2.txt#173; `blob:` VRP2.txt#1761). `javascript:`/`data:` URL confusion (VRP: `40059251`).
    *   **Policy Bypass (CSP, SameSite, Sandbox, Mixed Content, CORS):**
        *   *SameSite:* Web Share (VRP2.txt#11901), BackgroundFetch (VRP: `1244289`), Redirects (VRP2.txt#9752), SW FetchEvent (VRP: `1115438`), Android Intents (VRP: `1375132`), Prerender (VRP2.txt#14702). Cookie Parsing (VRP2.txt#13543). Drag/Drop `DownloadURL` (VRP: `40060358`). See [privacy.md](privacy.md).
        *   *CSP:* Service Worker (VRP: `598077`), `about:blank` (VRP2.txt#1924), `blob:` (VRP2.txt#7831), `filesystem:` (VRP2.txt#5009), `javascript:`+`srcdoc` (VRP2.txt#11413), Directive Handling (VRP: `1288035`, VRP2.txt#11841). See [content_security_policy.md](content_security_policy.md).
        *   *IFrame Sandbox:* `allow-popups-to-escape-sandbox` (VRP: `40069622`, `40057525`), `allow-downloads` (VRP: `40060695`), `intent://` (VRP: `1365100`), `javascript:`+`opener` (VRP: `1017879`), Top Nav Header (VRP2.txt#4247). Cross-origin iframe navigation bypass (VRP: `40053936`). See [iframe_sandbox.md](iframe_sandbox.md).
        *   *Mixed Content:* PWA/SW (VRP2.txt#8497). `javascript:` popup (VRP2.txt#9702).
        *   *CORS:* WebSockets (VRP2.txt#1467), CORB leak (VRP2.txt#13544), BackgroundFetch wildcard (VRP2.txt#9370), SW Origin header (VRP2.txt#11875).
        *   *Other:* `crossOriginIsolated` bypass (VRP: `40056434`).
    *   **Timing/Side-Channel Leaks:**
        *   *API State:* Performance API redirect info (VRP: `40054148`, VRP2.txt#13061), History/Nav (`history.length` VRP: `1208614`, `baseURI` VRP: `1329654`, `window.length` VRP: `40059056`). See [performance_apis.md](performance_apis.md), [history.md](history.md).
        *   *Resource Size:* BackgroundFetch (VRP: `1247376`), Cache API+Range (VRP2.txt#14773), Storage Estimate (VRP2.txt#12007). See [background_fetch.md](background_fetch.md).
        *   *Pixel/Rendering Timing:* Canvas `drawImage` (VRP2.txt#6693), CSS Filters (VRP2.txt#16788), SW+Canvas (VRP2.txt#7318), WebGL (VRP2.txt#16520, #13376, #16788). See [gpu.md](gpu.md).
        *   *History/Visited Links:* CSS Paint/Transitions (VRP: `680214`, VRP2.txt#12845).
        *   *Environment:* Env vars via pickers/downloads (VRP: `1247389`, VRP2.txt#1102). Keystroke timing (VRP: `1315899`). See [privacy.md](privacy.md).
    *   **API Misuse/Interaction:** PaymentRequest bypass after first call (VRP: `40072274`), PushMessaging (VRP: `1275626`), ContentIndex (VRP: `1263530`), Text Fragments (VRP: `1400345`), FSA+Downloads (VRP: `1428743`), `registerProtocolHandler` (VRP: `971188`), WebRTC port exhaust (VRP2.txt#6058), User Activation (VRP2.txt#9606), Fetch object leak (VRP2.txt#6873). Document PiP UI spoof via opener (VRP: `40062959`), resize/move (VRP: `40063071`). Android WebView JS execution from cross-origin iframe (VRP: `40052335`).

*   **Specific APIs:** WebTransport, WebCodecs ([webcodecs.md](webcodecs.md)), WebGPU ([webgpu.md](webgpu.md)), WebXR ([webxr.md](webxr.md)), FedCM ([fedcm.md](fedcm.md)), Web Share API ([webshare.md](webshare.md)), Web Bluetooth ([bluetooth.md](bluetooth.md)), Web USB ([webusb.md](webusb.md)), Web Serial ([webserial.md](webserial.md)), Portals ([portals.md](portals.md)), File System API ([file_system_access.md](file_system_access.md)), Push Messaging API ([push_messaging.md](push_messaging.md)), Translate API ([translation_ui.md](translation_ui.md)), PaymentRequest API ([payments.md](payments.md)), BackgroundFetch API ([background_fetch.md](background_fetch.md)), Content Index API ([content_index.md](content_index.md)), Text Fragments ([text_fragments.md](text_fragments.md)), FencedFrames ([fenced_frames.md](fenced_frames.md)), Service Workers ([service_workers.md](service_workers.md)), SharedWorker ([worker_threads.md](worker_threads.md)), `registerProtocolHandler` ([protocol_handler.md](protocol_handler.md)).

**6. Filesystem & Scheme Handling (Medium Likelihood, Variable VRP Value):**

*   **Focus:** Analyze handling of file interactions (`<input type=file>`, drag/drop, FSA), special URL schemes (`file://`, `filesystem://`, `content://`, `intent://`, `javascript:`, `data:`, `blob:`, `chrome*://`, `fido://`), and downloads. Look for boundary violations, info leaks, policy bypasses. See [file_system_access.md](file_system_access.md), [downloads.md](downloads.md), [intents.md](intents.md).
*   **Techniques & VRP Patterns:**
    *   **Local File Access/Disclosure:**
        *   *Extensions:* Debugger (VRP2.txt#6009, `Page.navigate` VRP: `40060173`), `downloads`+FSA (VRP: `1428743`), `input`+symlinks (VRP: `1378484`), `sourceMappingURL` (VRP: `1349146`), `registerProtocolHandler` (VRP: `971188`), `.url` files (VRP: `1303486`), `SharedWorker` (VRP2.txt#670).
        *   *Other:* File picker default (VRP: `756268`). Android Intents (VRP: `1198142`). FencedFrames (VRP: `1454937`). PDF plugin (VRP2.txt#2403). XSLT (VRP2.txt#8930). CSS injection (VRP2.txt#16042). `DOM.setFileInputFiles` (VRP2.txt#15188). Elevation Service LPE (VRP2.txt#6852). Fetch+History (VRP2.txt#16438). Drag/Drop (VRP2.txt#8707). Recent folder (VRP2.txt#3479). `window.location` (VRP2.txt#15110).
    *   **Special Scheme Handling:** `intent://` (Sandbox bypass VRP: `1365100`, Firebase dynamic link bypass VRP: `40064598`, Android app install spoof VRP: `41493134`). `android-app://` (VRP: `1092025`). `javascript:` (Origin confusion VRP: `40059251`, CSP bypass VRP: `1006188`, Self-XSS VRP: `850824`, SOP bypass VRP2.txt#11380, PiP spoof VRP2.txt#15301). `file://` (Nav bypass VRP: `40060173`, UNC path leak VRP: `1342722`, Symlink VRP2.txt#10231, Drag/Drop RCE VRP2.txt#11682). `chrome-untrusted://` (XSS VRP: `40057777`). `fido://` (Hijack VRP2.txt#3016). `content://` (Bypass VRP2.txt#1865). `chrome-devtools://` (Read C: VRP2.txt#12169). `chrome-error://` (Interstitial bypass VRP2.txt#12764). `tel:` (Spoof VRP2.txt#14950). `blob:` (SOP/Site Iso bypass VRP2.txt#1761, CSP bypass VRP2.txt#7831, Origin display VRP2.txt#16811). `filesystem:` (CSP bypass VRP2.txt#5009, Site Iso bypass VRP2.txt#6261).
    *   **Download Security:** Origin spoofing, Type masking (VRP: `1228653`), SB Bypass (VRP: `1416794`), OS Handler Abuse (VRP: `40060695` - iframe sandbox `allow-downloads`), Silent downloads (VRP: `1243802`), History manipulation (VRP: `1513412`). SameSite bypass via `DownloadURL` (VRP: `40060358`). See [safe_browsing_service.md](safe_browsing_service.md).

**7. Installer/Updater Security (Windows/macOS) (Medium Likelihood, High VRP Value - EoP Focus):**

*   **Focus:** Analyze installer/updater components (Google Update, Keystone) for EoP. Look for insecure file ops (temp dirs), races, permissions, IPC/COM/XPC flaws, signature bypasses. See [installer_security.md](installer_security.md).
*   **Techniques & VRP Patterns:** Temp dir file ops + links (VRP2.txt#1259, #9914, #1191, #1333, #12876), Races (VRP2.txt#157, #914, #1191), Permissions/ACLs (VRP2.txt#4151, #8739), COM Session Moniker (VRP2.txt#3763), Missing Impersonation (VRP2.txt#1152), Signature Bypass (VRP2.txt#914, #3063).

**8. General Code Analysis Techniques:**

*   **Fuzzing:** Target APIs, file parsers (PDF VRP: `40059101`), IPC/Mojo, codecs.
*   **Manual Code Review:** Focus on boundaries (IPC, process isolation, permissions), validation (URL, input), scheme handling, privilege management. Look for C++ vulns (UAF VRP: `40061678`, Type confusion VRP: `1337607`).
*   **Variant Analysis:** Use VRPs to find similar patterns.
*   **Exploit Technique Awareness:** Understand browser/OS exploit techniques.
*   **Corner Cases & Regressions:** Test edge cases, verify fixes.
*   **Platform & Context Specificity:** Test across platforms/contexts (WebView, Extensions, PWA).

**9. Data Transfer Boundaries & Trust (Medium-High Likelihood, Variable VRP Value):**

*   **Focus:** Analyze Drag/Drop, Copy/Paste, FSA. Look for origin/taint spoofing, permission timing issues, delegate bypasses, parsing flaws, context loss. See [drag_and_drop.md](drag_and_drop.md), [ipc.md](ipc.md), [file_system_access.md](file_system_access.md).
*   **Techniques & VRP Patterns:** Taint spoofing, Permission timing, Delegate bypass, Parsing complex formats (`fs/sources`), Context loss (SameSite via DownloadURL drag VRP: `40060358`), Copy/Paste XSS (mXSS VRP2.txt#900, #10912), Clipboard sanitizer bypass (VRP2.txt#10581).

**10. Parsing & Content Handling (Variable Likelihood/Impact):**

*   **Focus:** Analyze parsing of HTML, CSS, JS, XML, SVG, media, fonts. Look for parser differentials (mXSS), state confusion, mishandling malformed data, insecure interactions.
*   **Techniques & VRP Patterns:** mXSS (DOMPurify bypass VRP2.txt#4643, clipboard VRP2.txt#14643), Scheme/Type handling (iOS XML/SVG render VRP2.txt#8287, CSS injection VRP2.txt#16042), Resource Leaks (PDF `getThumbnail()` page count leak VRP: `40059101`, JS errors VRP2.txt#5947, CSP reports VRP2.txt#15887). See [plugin_security.md](plugin_security.md).

**11. Commit Validation & Transient State (Medium Likelihood, Variable VRP Value):**

*   **Focus:** Scrutinize `RenderFrameHostImpl::ValidateDidCommitParams`. Look for timing/state inconsistencies (opener vs popup ProcessLock), commit parameter mismatches, policy enforcement timing issues. See [navigation.md](navigation.md), [site_instance.md](site_instance.md), [process_lock.md](process_lock.md).
*   **Techniques & VRP Patterns:** Transient state confusion allowing checks based on opener's context (VRP: `40059251` - origin confusion for `javascript:`/`data:` URLs opened by cross-origin iframe). Edge cases in parameter validation (schemes, sandbox). Policy enforcement timing.

**Wiki Pages (Grouped by Research Tip & Prioritized - *Adjust Dynamically Based on Research*):**

*   **1. UI Security (High Priority):**
    *   High: [omnibox.md](omnibox.md), [permissions.md](permissions.md), [autofill.md](autofill.md), [picture_in_picture.md](picture_in_picture.md), [payments.md](payments.md), [fedcm.md](fedcm.md)
    *   Medium: [input.md](input.md), [navigation.md](navigation.md), [url_formatting.md](url_formatting.md), [web_app_identity.md](web_app_identity.md), [protocol_handler.md](protocol_handler.md), [webusb.md](webusb.md), [bluetooth.md](bluetooth.md), [plugin_security.md](plugin_security.md), [downloads.md](downloads.md), [portals.md](portals.md), [fenced_frames.md](fenced_frames.md), [drag_and_drop.md](drag_and_drop.md), [eye_dropper.md](eye_dropper.md)
    *   Lower: [side_panel.md](side_panel.md) (*Note: Side Panel UAF was High VRP*), [payment_handler.md](payment_handler.md), [popup_blocker.md](popup_blocker.md), [presentation_request_notification.md](presentation_request_notification.md), [render_widget_host_view_android.md](render_widget_host_view_android.md), [render_widget_host_view_aura.md](render_widget_host_view_aura.md), [screen_capture.md](screen_capture.md), [screen_sharing.md](screen_sharing.md), [tabs.md](tabs.md), [url_info.md](url_info.md), [user_notes.md](user_notes.md)

*   **2. Extension & DevTools Security (High Priority):**
    *   High: [extension_security.md](extension_security.md), [extensions_debugger_api.md](extensions_debugger_api.md), [devtools.md](devtools.md)
    *   Medium: [permissions.md](permissions.md), [downloads.md](downloads.md), [file_system_access.md](file_system_access.md), [content_security_policy.md](content_security_policy.md), [policy.md](policy.md)
    *   Lower: [extension_install_dialog.md](extension_install_dialog.md), [extensions_declarative_net_request_api.md](extensions_declarative_net_request_api.md), [extensions_web_request_api.md](extensions_web_request_api.md), [extensions_webrtc_audio_private_api.md](extensions_webrtc_audio_private_api.md), [native_messaging.md](native_messaging.md)

*   **3. Autofill & Input Handling (High Priority):**
    *   High: [autofill.md](autofill.md)
    *   Medium: [input.md](input.md), [eye_dropper.md](eye_dropper.md), [pointer_lock.md](pointer_lock.md)
    *   Lower:

*   **4. IPC/Mojo Security (Medium Priority):**
    *   Medium: [ipc.md](ipc.md), [mojo.md](mojo.md)
    *   Lower: [process_lock.md](process_lock.md) (*Related to commit validation*), [child_process_security_policy_impl.md](child_process_security_policy_impl.md), [guest_view_security.md](guest_view_security.md), [render_process_host.md](render_process_host.md)

*   **5. Web API & Feature Security (Medium Priority):**
    *   High: [site_isolation.md](site_isolation.md) (*Implicitly related*)
    *   Medium: [privacy.md](privacy.md), [content_security_policy.md](content_security_policy.md), [iframe_sandbox.md](iframe_sandbox.md), [fedcm.md](fedcm.md), [portals.md](portals.md), [fenced_frames.md](fenced_frames.md), [service_workers.md](service_workers.md), [payments.md](payments.md), [intents.md](intents.md), [webserial.md](webserial.md), [webusb.md](webusb.md), [bluetooth.md](bluetooth.md), [coop.md](coop.md) (*Related*)
    *   Lower: [performance_apis.md](performance_apis.md), [history.md](history.md), [background_fetch.md](background_fetch.md), [gpu.md](gpu.md), [webcodecs.md](webcodecs.md), [webgpu.md](webgpu.md), [webxr.md](webxr.md), [webshare.md](webshare.md), [push_messaging.md](push_messaging.md), [translation_ui.md](translation_ui.md), [content_index.md](content_index.md), [text_fragments.md](text_fragments.md), [worker_threads.md](worker_threads.md), [protocol_handler.md](protocol_handler.md), [cross_origin_resource_policy.md](cross_origin_resource_policy.md), [gpu_process.md](gpu_process.md), [indexed_db.md](indexed_db.md), [permissions_policy.md](permissions_policy.md), [quic.md](quic.md), [resource_load_scheduler.md](resource_load_scheduler.md), [resource_management.md](resource_management.md), [storage.md](storage.md), [webauthn.md](webauthn.md), [webrtc.md](webrtc.md), [websockets.md](websockets.md)

*   **6. Filesystem & Scheme Handling (Medium Priority):**
    *   Medium: [file_system_access.md](file_system_access.md), [downloads.md](downloads.md), [intents.md](intents.md), [safe_browsing_service.md](safe_browsing_service.md)
    *   Lower: [url_formatting.md](url_formatting.md), [url_utilities.md](url_utilities.md)

*   **7. Installer/Updater Security (High Priority):**
    *   High: [installer_security.md](installer_security.md)
    *   Medium:
    *   Lower:

*   **8. General Code Analysis Techniques (N/A - Methodology)**

*   **9. Data Transfer Boundaries & Trust (Medium Priority):**
    *   Medium: [drag_and_drop.md](drag_and_drop.md), [ipc.md](ipc.md), [file_system_access.md](file_system_access.md)
    *   Lower:

*   **10. Parsing & Content Handling (Variable Priority):**
    *   Lower: [plugin_security.md](plugin_security.md) (*PDF*), [printing.md](printing.md), [spellcheck.md](spellcheck.md)

*   **11. Commit Validation & Transient State (Medium Priority):**
    *   Medium: [navigation.md](navigation.md), [site_instance.md](site_instance.md), [process_lock.md](process_lock.md)
    *   Lower: [browsing_instance.md](browsing_instance.md), [frame_tree.md](frame_tree.md), [navigation_request.md](navigation_request.md), [render_frame_host_impl.md](render_frame_host_impl.md), [site_info.md](site_info.md), [site_instance_group.md](site_instance_group.md)

*   **Other/Uncategorized:**
    *   High: [android_webview.md](android_webview.md) (*WebView*)
    *   Medium: [privacy.md](privacy.md) (*General*)
    *   Lower: [infra_luci.md](infra_luci.md) (*Build/Infra*), [android_webview_app_defined_websites.md](android_webview_app_defined_websites.md)

By focusing on these VRP-informed areas and techniques, and by continuously updating the wiki, we can improve Chromium security research effectiveness.