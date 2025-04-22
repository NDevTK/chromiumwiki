# Component: Privacy

## 1. Component Focus
*   **Functionality:** Encompasses features and mechanisms designed to protect user privacy, including Incognito Mode, cookie handling (especially SameSite), storage partitioning and isolation, tracking prevention techniques (e.g., partitioning, bounce tracking mitigation), permissions with privacy implications (Geolocation, Camera, Mic), and APIs developed with privacy considerations (e.g., FedCM, Privacy Sandbox APIs).
*   **Key Logic:** Defining and enforcing isolation boundaries (Incognito profiles, storage partitions), implementing cookie policies (`SameSite`), managing permissions for sensitive data access, sanitizing or restricting potentially identifying information, implementing privacy-preserving APIs.
*   **Core Files:**
    *   `chrome/browser/profiles/`: Profile management, including Incognito (`profile_impl.cc`, `off_the_record_profile_impl.cc`).
    *   `content/browser/storage_partition_impl.*`: Core implementation of storage isolation.
    *   `net/cookies/`: Cookie parsing, storage (`cookie_monster.cc`), and policy enforcement (`cookie_constants.h`, `cookie_access_delegate.h`).
    *   `services/network/public/cpp/features.cc`: Feature flags related to network state partitioning, SameSite behavior.
    *   `services/network/cookie_manager.cc`: Handles cookie access requests.
    *   `components/permissions/`: Permission management framework. See [permissions.md](permissions.md).
    *   `components/content_settings/`: Storage for content settings, including cookie controls.
    *   Privacy Sandbox APIs (various locations, e.g., `components/attribution_reporting`, `components/topics`, `components/fledge`).
    *   FedCM (`content/browser/webid/`). See [fedcm.md](fedcm.md).

## 2. Potential Logic Flaws & VRP Relevance
Privacy vulnerabilities often involve bypassing intended isolation or protection mechanisms, leading to data leakage or unexpected tracking.

*   **Cookie Policy Bypass (SameSite Focus):** Circumventing `SameSite=Lax` or `SameSite=Strict` restrictions, allowing cookies to be sent in cross-site contexts where they shouldn't be, potentially enabling CSRF or information leakage.
    *   **VRP Pattern (Interaction with other Features):** SameSite bypasses often occur due to interactions with other web platform features that initiate cross-site requests in ways not fully accounted for by the SameSite checks.
        *   *Web Share API:* (VRP2.txt#11901). See [webshare.md](webshare.md).
        *   *BackgroundFetch:* (VRP: `1244289`; VRP2.txt#10929, #10871). See [background_fetch.md](background_fetch.md).
        *   *Service Worker FetchEvent:* Worker-initiated fetch bypassing checks (VRP: `1115438`; VRP2.txt#7521). See [service_workers.md](service_workers.md).
        *   *Android Intents:* Navigations initiated via Android Intents (VRP: `1375132`; VRP2.txt#10199, #5620). See [intents.md](intents.md).
        *   *Prerender/Prefetch:* Requests initiated during prerendering bypassing checks (VRP2.txt#14702, #16021, #14976).
    *   **VRP Pattern (Redirects):** Specific redirect sequences or types allowing SameSite cookies to be attached to the redirected cross-site request (VRP: `698492`; VRP2.txt#9752). See [navigation.md](navigation.md).
    *   **VRP Pattern (Cookie Parsing):** Incorrect parsing allowing malformed cookies (e.g., `=__Host-`) to bypass prefix restrictions (`__Host-`, `__Secure-`) enforced by HTTP checks (VRP2.txt#13543).

*   **Information Leaks (Side-Channels, API Misuse):** Features or APIs unintentionally leaking cross-origin or sensitive user information.
    *   **VRP Pattern (Resource Size/Timing Leaks):** Leaking information about cross-origin resources via timing or size oracles.
        *   *BackgroundFetch:* Leaking response size (VRP: `1247376`, `1260649`, `1267311`; VRP2.txt#10920, #10690, #10871). See [background_fetch.md](background_fetch.md).
        *   *Cache API + Range Requests:* Leaking size (VRP2.txt#14773).
        *   *`navigator.storage.estimate`:* Padding bypass leaking storage usage (VRP2.txt#12007).
        *   *Performance API:* Leaking redirect info (`nextHopProtocol`) or timing (VRP: `40054148`; VRP2.txt#13061, #14397). See [performance_apis.md](performance_apis.md).
        *   *Canvas/WebGL Timing:* `drawImage`, filters, composite ops leaking pixel data via timing (VRP: `1093099`, `716057`; VRP2.txt#6693, #7318, #7346, #16520, #16755, #16585, #13376, #16788). See [gpu.md](gpu.md).
    *   **VRP Pattern (History/Visited Link Leaks):** Leaking visited status via CSS or timing attacks.
        *   *SoftNav+Paint:* (VRP: `1459093`). See [navigation.md](navigation.md).
        *   *CSS Paint API:* (VRP: `680214`).
        *   *CSS Transitions:* (VRP: `1211002`; VRP2.txt#12845).
    *   **VRP Pattern (API State Leaks):** APIs revealing state across origins.
        *   *`history.length` / `document.baseURI` / `window.length`:* Leaking navigation history details (VRP: `1208614`, `1329654`, `40059056`; VRP2.txt#5110, #15203, #275). See [history.md](history.md), [navigation.md](navigation.md).
    *   **VRP Pattern (Environment Variable Leaks):** File pickers or download dialogs expanding environment variables (`%VAR%`) in suggested filenames, leaking potentially sensitive system info (VRP: `1247389`, `1310461`, `1322058`, `1310462`; VRP2.txt#1102, #2919, #2935, #2980, #3189, #5603). See [downloads.md](downloads.md), [file_system_access.md](file_system_access.md).
    *   **VRP Pattern (Keystroke Timing):** Leaking typed keys via precise timing analysis of input event handling (VRP: `1315899`; VRP2.txt#4417). See [input.md](input.md).

*   **Incognito Mode Leaks/Bypass:** Data (cookies, history, storage, state) unexpectedly leaking from or persisting beyond Incognito sessions, or extensions improperly accessing Incognito state.
    *   **VRP Pattern (Extension Access):** Extensions (e.g., using `chrome.debugger` with `targetId`) gaining access to Incognito tabs/state without the necessary "split" mode manifest setting or explicit user permission. (VRP: `40056776`).
    *   **VRP Pattern (Incognito Prompt Tapjacking - Android):** Bypassing interaction requirements for the "Close Incognito tabs" prompt (VRP2.txt#15326).

*   **Storage Isolation Failures:** Data leaking between different Storage Partitions (e.g., top-level site vs. embedded third-party iframe). Requires auditing how storage keys are generated and enforced for all storage types (Cookies, LocalStorage, IndexedDB, Cache API, etc.).

*   **Permission Bypass/Confusion (Privacy Related):** Exploits leading to permissions being granted without proper user awareness, especially for privacy-sensitive permissions (Geolocation, Camera, Mic). Often involves UI spoofing or interaction bypasses.
    *   **VRP Pattern (Autofill Interaction):** Autofill UI issues classified under "Privacy" likely involve bypassing user interaction requirements through timing or focus manipulation. (VRP: `40060134`, `40058217`, `40056900`). See [autofill.md](autofill.md).

## 3. Further Analysis and Potential Issues
*   **SameSite Enforcement Points:** Where exactly is SameSite policy enforced (`net::CookieAccessDelegate`, `net::CookieMonster`, network service)? Are all navigation types (prerender, redirects, service worker fetches, BackgroundFetch, intents) correctly classified and checked?
*   **Incognito Data Separation:** Audit all storage mechanisms (cookies, LocalStorage, IDB, Cache, etc.) and browser state (history, permissions) to ensure strict separation and timely cleanup for Incognito profiles. How do extensions interact with Incognito state?
*   **Storage Partitioning:** Verify that storage keys (`storage::StorageKey`) are correctly generated and enforced for all storage APIs, preventing cross-partition access.
*   **Side-Channel Mitigations:** Review mitigations against timing/size leaks (e.g., cache padding). Are they sufficient? Are there other potential side channels (CPU usage, network timing variations)?
*   **Privacy Sandbox APIs:** Analyze the design and implementation of Privacy Sandbox features (Topics, Fledge, Attribution Reporting) for potential privacy leaks or bypasses of intended restrictions.
*   **Fingerprinting Surface:** Continuously evaluate new APIs and features for their potential contribution to browser fingerprinting.

## 4. Code Analysis
*   `net/cookies/cookie_monster.cc`: Core cookie storage and retrieval logic. Includes SameSite enforcement checks (`ComputeCookieInclusionStatus`).
*   `net/cookies/cookie_access_delegate.h`: Interface for embedder-specific cookie access rules (e.g., blocking third-party cookies).
*   `services/network/cookie_manager.cc`: Network service component managing cookie access.
*   `content/browser/storage_partition_impl.cc`: Manages storage isolation based on `storage::StorageKey`.
*   `chrome/browser/profiles/profile_impl.cc`, `off_the_record_profile_impl.cc`: Profile and Incognito profile implementation.
*   `components/safe_browsing/core/browser/realtime/url_lookup_service.cc`: Example of component potentially accessing URLs that needs privacy consideration.
*   `chrome/browser/privacy_sandbox/`: Implementation of Privacy Sandbox features.
*   Various API implementations check context validity (`IsContextValidAndSecure`) and permissions.

## 5. Areas Requiring Further Investigation
*   **Thorough audit of SameSite cookie implementation:** Focus on interactions with redirects (server/client), Service Workers (`FetchEvent`), Background Fetch, Prerender, Android Intents, and potentially WebTransport/WebSockets.
*   **Investigation of data isolation guarantees for Incognito mode:** Check extension interactions, file system access, network state, and any potential OS-level leaks.
*   **Analysis of potential side-channels in all storage mechanisms:** Including timing, size (padding effectiveness), error messages, and resource usage.
*   **Security review of Privacy Sandbox features:** Fledge, Topics, Attribution Reporting, etc., for unintended data leakage or tracking vectors.
*   **File Picker / Download Environment Variable Leaks:** Ensure robust sanitization in all paths where filenames can be suggested or manipulated, especially cross-platform.

## 6. Related VRP Reports
*   **SameSite Bypass:** VRP: `698492`, `1115438`, `1244289`, `1375132`; VRP2.txt#9752 (Redirect), #7521 (SW Fetch), #10929, #10871 (BackgroundFetch), #11901 (WebShare), #10199 (Intent), #5620 (Intent), #14702, #16021, #14976 (Prerender).
*   **Cookie Prefix Bypass:** VRP2.txt#13543 (`=` prepend).
*   **Info Leaks (Size/Timing):** VRP: `1247376`, `1260649`, `1267311`, `40054148`, `1093099`, `716057`, `1211002`, `1459093`, `680214`; VRP2.txt#10920, #10690, #10871 (BackgroundFetch), #14773 (Cache+Range), #12007 (Storage Estimate), #14397, #13061 (Perf API), #6693, #7318, #7346, #16520, #16755, #16585, #13376, #16788 (Canvas/WebGL), #12845 (CSS Transitions).
*   **Info Leaks (History/State):** VRP: `1208614`, `1329654`, `40059056`; VRP2.txt#5110 (`history.length`), #15203 (`baseURI`), #275 (`window.length`), #5817 (Load event timing).
*   **Info Leaks (Environment Variables):** VRP: `1247389`, `1310461`, `1322058`, `1310462`; VRP2.txt#1102, #2919, #2935, #2980, #3189, #5603 (Downloads/File Picker).
*   **Info Leaks (Keystroke Timing):** VRP: `1315899`; VRP2.txt#4417.
*   **Incognito Bypass:** VRP: `40056776` (Debugger access); VRP2.txt#15326 (Tapjacking prompt).
*   **Autofill (Privacy Context):** VRP: `40060134`, `40058217`, `40056900`.

## 7. Cross-References
*   [navigation.md](navigation.md)
*   [service_workers.md](service_workers.md)
*   [background_fetch.md](background_fetch.md)
*   [intents.md](intents.md)
*   [performance_apis.md](performance_apis.md)
*   [history.md](history.md)
*   [gpu.md](gpu.md)
*   [downloads.md](downloads.md)
*   [file_system_access.md](file_system_access.md)
*   [input.md](input.md)
*   [extension_security.md](extension_security.md)
*   [autofill.md](autofill.md)
*   [fedcm.md](fedcm.md)