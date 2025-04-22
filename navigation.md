# Component: Navigation & Site Instance Selection

## 1. Component Focus
*   **Functionality:** Governs how Chromium handles navigations (user-initiated, script-initiated, redirects, etc.), determines the appropriate security context (`SiteInstance`, `BrowsingInstance`, `ProcessLock`), selects or creates the target `RenderProcessHost`, enforces navigation-related security policies (Site Isolation, CSP, COOP/COEP, SameSite), manages session history, and updates the browser UI (Omnibox, security indicators).
*   **Key Logic:** Navigation initiation (`FrameLoader`, `NavigatorImpl`), creation and management of `NavigationRequest`, SiteInstance selection (`RenderFrameHostManager`, `SiteInstanceImpl`), BrowsingInstance management, process selection/reuse (`RenderProcessHostImpl`), policy enforcement (`ChildProcessSecurityPolicyImpl`, COOP/COEP checks, CSP checks), redirect handling, commit validation (`RenderFrameHostImpl::ValidateDidCommitParams`), session history management (`NavigationControllerImpl`), UI updates (`NavigationControllerDelegate`).
*   **Core Files:**
    *   `content/browser/renderer_host/navigation_request.cc`: Represents a single navigation, holds state, handles redirects, prepares for commit. See [navigation_request.md](navigation_request.md) for details.
    *   `content/browser/renderer_host/render_frame_host_manager.cc`: Manages RFHs, central orchestrator for SiteInstance selection during navigation. Contains `GetSiteInstanceForNavigation`.
    *   `content/browser/site_instance_impl.*`: Represents a security principal within a `BrowsingInstance`. Key for Site Isolation. See [site_instance.md](site_instance.md).
    *   `content/browser/browsing_instance.*`: Groups related SiteInstances, defines scripting boundaries. See [browsing_instance.md](browsing_instance.md).
    *   `content/browser/process_lock.*`: Represents the set of origins locked to a specific `RenderProcessHost`. See [process_lock.md](process_lock.md).
    *   `content/browser/renderer_host/render_process_host_impl.*`: Manages the renderer process.
    *   `content/browser/child_process_security_policy_impl.*`: Enforces process capabilities (URL access, scheme restrictions).
    *   `content/browser/web_contents/web_contents_impl.cc`: Top-level browser content abstraction. Handles popup creation (`CreateNewWindow`).
    *   `content/browser/renderer_host/render_frame_host_impl.cc`: Handles navigation commit (`CommitNavigation`) and validation (`ValidateDidCommitParams`).
    *   `content/public/browser/navigation_controller.h`, `content/browser/web_contents/navigation_controller_impl.cc`: Manages session history.
    *   `third_party/blink/renderer/core/loader/frame_loader.cc`: Initiates navigations from Blink.
    *   `third_party/blink/renderer/core/loader/document_loader.cc`: Handles loading and committing documents in Blink.
    *   `components/url_formatter/`: URL formatting for display. See [url_formatting.md](url_formatting.md).
    *   `chrome/browser/ui/views/omnibox/`: Omnibox UI implementation. See [omnibox.md](omnibox.md).
    *   `net/url_request/`, `services/network/`: Network stack handling requests/redirects.

## 2. SiteInstance Selection Overview
(See `RenderFrameHostManager::GetSiteInstanceForNavigation` for detailed logic)

The core logic determines if a navigation requires a new `BrowsingInstance` (breaking script connections) or a new `SiteInstance` (process isolation) based on factors like cross-origin status, security policies (COOP, WebUI transitions), navigation type (history, new window), and performance optimizations (BackForwardCache proactive swaps). The goal is to place the navigation in the correct security context. Flaws here can lead to process misallocation or broken features.

## 3. Potential Logic Flaws & VRP Relevance

Navigation is a complex process with many potential pitfalls, frequently leading to security vulnerabilities.

*   **URL Spoofing (High Likelihood, Medium-High VRP):** Causing the Omnibox or other security UI to display a misleading URL or origin.
    *   **VRP Pattern (Crash Recovery):** Navigating or activating a portal/frame immediately after its renderer crashes can lead to the old (pre-crash) URL being displayed while attacker content loads. (VRP: `40057561`, `40064170` - Portals; VRP2.txt#9502, #10177, #14588, #14899 - Downloads page context).
    *   **VRP Pattern (Redirect Timing/State):** Exploiting timing windows during redirects, especially involving downloads or `beforeunload` handlers, to corrupt the NavigationEntry or Omnibox state. (VRP2.txt#11643 - download redirect + `beforeunload`, #16678 - subframe nav + history corruption).
    *   **VRP Pattern (Scheme Handling - Android):** Incorrect parsing or display logic on Android when specific schemes or scheme-like strings appear later in the URL. (VRP: `40072988`; VRP2.txt#1652).
    *   **VRP Pattern (Scroll/Timing - Android):** Interactions between scrolling, tab switching, slow navigations, and omnibox visibility updates causing the wrong URL to persist. (VRP: `379652406`; VRP2.txt#2851, #3274).
    *   **VRP Pattern (iOS Specific):** Exploiting modal dialogs or navigation manager quirks on iOS. (VRP2.txt#8057, #13355).
    *   **VRP Pattern (Interstitial Bypass):** Overwriting interstitial pages (e.g., Safe Browsing warnings) with attacker content while retaining the interstitial's URL/state transiently. (VRP2.txt#11587).
    *   **VRP Pattern (Special URLs):** Using long URLs (`data:`, `about:blank#...`, long subdomains) to confuse elision logic or cause UI state errors. (VRP2.txt#12072, #4130, #8323, #8581).
    *   **VRP Pattern (`javascript:` URLs):** Using `javascript:` URLs combined with navigation timing to execute script while spoofing another origin. (VRP2.txt#5966, #6008).
    *   **VRP Pattern (External Protocol Dialogs):** Server-side redirects to external protocols (`tel:`, `mailto:`, custom schemes) showing the *initiating* page's origin in the confirmation dialog, not the redirecting URL's origin. (VRP: `40055515`; VRP2.txt#9087, #13592, #11016, #13605, #13555, #15881). See [protocol_handler.md](protocol_handler.md).
    *   **VRP Pattern (Download Origin):** Incorrect origin displayed in download UI due to redirects or special schemes (`about:srcdoc`). (VRP: `916831`, `11062`; VRP2.txt#2294, #11062, #12168). See [downloads.md](downloads.md).
    *   **VRP Pattern (IDN/RTL):** Using RTL characters or confusable scripts (e.g., Cyrillic 'a' vs Latin 'a') to spoof domains. See [url_formatting.md](url_formatting.md).

*   **Origin Confusion / SOP Bypass (High Likelihood, High VRP):** Tricking the browser into treating different origins as the same or applying the wrong origin's privileges.
    *   **VRP Pattern (Popup Timing - VRP 40059251):** Popups initially inheriting opener's SiteInstance/ProcessLock. Browser-side checks made *before* the popup commits to its final (potentially opaque/sandboxed) origin might use the opener's privileges incorrectly. See Section 5 (`ValidateDidCommitParams`).
    *   **VRP Pattern (`javascript:` URLs):** Similar to spoofing, using `javascript:` URLs opened in new contexts to potentially inherit incorrect origins or execute script with wrong privileges. (VRP: `40059251`; VRP2.txt#173).
    *   **VRP Pattern (`blob:` URLs):** Issues with origin inheritance or display for blob URLs, potentially leading to SOP bypass. (VRP2.txt#11681, #1761 - Site Iso bypass, #679).
    *   **VRP Pattern (Cross-Origin Read via History):** Chaining navigation timing, history manipulation (`history.back`), and potentially redirects to read cross-origin data. (VRP2.txt#16438).

*   **Policy Bypass (CSP, SameSite, Sandbox, etc.) (High Likelihood, Medium-High VRP):** Using navigation mechanics to circumvent security policies.
    *   **VRP Pattern (CSP Bypass via Navigation):** Navigating frames to specific schemes (`about:blank`, `blob:`, `filesystem:`, `about:srcdoc`, `javascript:`) that incorrectly inherit or fail to apply the initiator's CSP. (VRP2.txt#1924, #7831, #11985, #5009, #11413, #1185, #8077). See [content_security_policy.md](content_security_policy.md).
    *   **VRP Pattern (SameSite Bypass via Navigation):** Using redirects, Service Worker FetchEvents, Android Intents, or Prerendering initiated navigations to bypass SameSite cookie restrictions. (VRP2.txt#9752, #7521, #10199, #14702, #16021, #14976). See [privacy.md](privacy.md), [service_workers.md](service_workers.md), [intents.md](intents.md).
    *   **VRP Pattern (Sandbox Bypass via Navigation):** Exploiting redirects (`Content-Security-Policy: sandbox allow-top-navigation` header) or specific schemes (`intent://`) to bypass `allow-top-navigation` or other sandbox restrictions. (VRP: `40053936`; VRP2.txt#4247, #4910, #8387, #8121, #15706, #7507). See [iframe_sandbox.md](iframe_sandbox.md).
    *   **VRP Pattern (Mixed Content Bypass via Navigation):** Using `javascript:` popups. (VRP2.txt#9702).

*   **Information Leaks (Navigation Timing/State) (Medium Likelihood, Medium VRP):** Leaking cross-origin information through side channels related to navigation timing or state.
    *   **VRP Pattern (History Length/State):** Leaking information about visited state or navigation history via `history.length`, `document.baseURI`, `window.length`, or load event timing. (VRP: `1208614`, `1329654`, `40059056`; VRP2.txt#5110, #15203, #275, #5817). See [history.md](history.md).
    *   **VRP Pattern (Performance API Leaks):** Leaking redirect URLs or timing information via `PerformanceNavigationTiming` entries (`nextHopProtocol`). (VRP: `40054148`; VRP2.txt#14397, #13061). See [performance_apis.md](performance_apis.md).
    *   **VRP Pattern (Error/CSP Reports):** Leaking post-redirect URLs via error message stack traces or CSP violation reports (`blockedURI`). (VRP2.txt#5947, #15887).

*   **SiteInstance/Process Model Flaws (Low Likelihood, High VRP):** Incorrect decisions in `GetSiteInstanceForNavigation` leading to Site Isolation bypasses (placing cross-site documents in the same process). (Less common recently, but fundamental).

## 4. Commit Validation (`RenderFrameHostImpl::ValidateDidCommitParams`)
*   **Role:** Acts as the final browser-side security check before a navigation commit is finalized. Runs *after* the renderer claims a commit has happened but *before* the browser updates its internal state (like the `NavigationEntry`'s URL/origin or the `ProcessLock`).
*   **Key Checks:**
    *   Verifies the renderer's claimed origin (`params.origin`) matches the browser's expected origin (`origin_to_commit` calculated during `NavigationRequest`).
    *   Calls `ChildProcessSecurityPolicyImpl::CanCommitOriginAndUrl` to ensure the *committing process* (identified by its `ProcessLock`) has the capability to host the claimed origin and URL.
    *   Checks for illegal navigations (e.g., navigating subframes to `about:blank` inappropriately).
    *   Validates other parameters (`transition`, `gesture`, MHTML state, etc.).
*   **VRP Relevance (VRP 40059251):** Vulnerabilities related to transient states (like popups initially inheriting opener's process) often rely on bypassing these final validation checks or exploiting edge cases where the browser's `origin_to_commit` or the process's `ProcessLock` state is incorrect *at the time of validation*. The validation must be robust against renderer manipulation and accurately reflect the intended final security context.

## 5. URL Display/Omnibox Security
*   **Key Components:** `OmniboxViewViews`, `LocationBarView`, `OmniboxEditModel`, `AutocompleteController`, `url_formatter`.
*   **Challenges:** Accurately reflecting the security context of the current page, handling complex URLs (IDN, RTL, long URLs, special schemes), preventing spoofing during navigations/errors/interstitials, eliding URLs securely.
*   **VRP Patterns:**
    *   **Scheme/Redirect Issues:** Incorrect URL display after specific redirects or with certain schemes (VRP: `40072988`, `40055515`; VRP2.txt#1652, #11643, #16716, #16678).
    *   **Timing/State Issues (Android):** Scroll/tab-switch interactions leading to stale URL display (VRP: `379652406`; VRP2.txt#2851, #3274, #9427).
    *   **Crash/Error States:** Stale URL persisting after crashes or during interstitial display (VRP: `40057561`; VRP2.txt#9502, #7679, #11587).
    *   **IDN/RTL/Confusables:** Exploiting lookalike characters or RTL rendering bugs (VRP2.txt#15139, #12423, #1618, #16429, #15140, #16501). See [url_formatting.md](url_formatting.md).
    *   **Long URLs/Subdomains:** Incorrect elision or display with overly long components (VRP2.txt#12072, #8323, #8581).
*   **Key Defenses:** Using `url_formatter::FormatUrlForSecurityDisplay`, careful state management in `NavigationControllerImpl`, correct handling of `NavigationEntry` updates, robust IDN display logic.

## 6. Further Analysis and Potential Issues
*   **Redirect Handling:** Audit how security policies (CSP, SameSite, Sandbox) are re-evaluated and applied after *server-side* and *client-side* redirects. Are sandbox flags or initiator origins correctly propagated? (VRP: `40053936`, VRP2.txt#9752).
*   **Navigation Commit Validation:** Deep dive into `ValidateDidCommitParams`. Are there edge cases with specific schemes (`data:`, `blob:`, `javascript:`, `about:srcdoc`), iframe types (sandboxed, fenced), or navigation types (popups, history) where the validation might be bypassed or use incorrect state? How does it interact with `ProcessLock` updates?
*   **Special Scheme Handling:** Systematically review how navigations to/from `data:`, `blob:`, `filesystem:`, `javascript:`, `about:blank`, `about:srcdoc` inherit origins, CSPs, and sandbox flags. Are checks in `ChildProcessSecurityPolicyImpl::CanCommitOriginAndUrl` sufficient?
*   **Android Navigation Logic:** Review Android-specific navigation paths (Intents, Custom Tabs) for correct policy enforcement and UI state management.
*   **Omnibox State Synchronization:** Ensure the Omnibox UI state is always correctly synchronized with the `NavigationController`'s committed entry, especially during complex transitions (redirects, errors, crashes, interstitials, downloads).
*   **Information Leak Vectors:** Re-evaluate `PerformanceNavigationTiming`, `history`, and error reporting mechanisms for potential cross-origin leaks during navigation.

## 7. Related VRP Reports
*(See extensive list categorized under patterns in Section 3)*

## 8. Cross-References
*   [navigation_request.md](navigation_request.md)
*   [site_instance.md](site_instance.md)
*   [browsing_instance.md](browsing_instance.md)
*   [site_info.md](site_info.md)
*   [url_info.md](url_info.md)
*   [process_lock.md](process_lock.md)
*   [site_isolation.md](site_isolation.md)
*   [coop_coep.md](coop_coep.md)
*   [content_security_policy.md](content_security_policy.md)
*   [iframe_sandbox.md](iframe_sandbox.md)
*   [privacy.md](privacy.md) (SameSite Cookies)
*   [omnibox.md](omnibox.md)
*   [url_formatting.md](url_formatting.md)
*   [history.md](history.md)
*   [performance_apis.md](performance_apis.md)
*   [intents.md](intents.md) (Android)
*   [protocol_handler.md](protocol_handler.md)
*   [downloads.md](downloads.md)