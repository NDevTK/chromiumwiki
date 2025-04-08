# Component: Site Isolation

## 1. Component Focus
*   **Functionality:** A core security architecture in Chromium designed to mitigate threats like Spectre and compromise by rendering content from different "sites" (usually eTLD+1) in separate sandboxed renderer processes. This prevents one site's process from directly accessing another site's data or interacting with it in unintended ways.
*   **Key Logic:** Process allocation based on Site URL (`SiteInfo`, `SiteInstance`), enforcement of process locks (`RenderProcessHostImpl`, `ChildProcessSecurityPolicyImpl`), secure handling of navigations and redirects (`NavigationRequest`), isolation of storage (`StoragePartition`), secure inter-process communication (IPC/Mojo).
*   **Scope:** This page provides an overview focusing on the logic connecting various components to achieve isolation. For deeper details on specific parts, refer to linked pages.

## 2. Potential Logic Flaws & VRP Relevance
Failures in site isolation logic can lead to severe security vulnerabilities, including SOP bypass, cross-site data leakage, and renderer compromise escalation. Key areas include:

*   **Incorrect Site/Origin Determination:** Errors in classifying URLs into sites (`SiteInfo`) or origins (`UrlInfo`), especially for special schemes or edge cases, leading to incorrect process placement.
    *   **VRP Relevance:** Issues with `blob:`, `filesystem:`, `data:`, `about:srcdoc`, `javascript:` leading to origin confusion or incorrect process sharing (VRP: `40059251`; VRP2.txt#173, #542, #665, #1508, #1690, #1761, #12127).
*   **Process Allocation/Reuse Errors:** Placing navigations into the wrong process due to flawed suitability checks (`RenderProcessHostImpl::MayReuseAndIsSuitable`) or incorrect `SiteInstance` management.
    *   **VRP Relevance:** Reuse of non-sandboxed context for sandboxed iframe (VRP2.txt#6788). Issues related to process sharing involving WebUI bindings (VRP: `40053875`). Data URL process placement after tab restore (VRP2.txt#11669).
*   **Process Lock Bypass/Errors:** Failure to correctly apply or enforce process locks (`RenderProcessHostImpl::SetProcessLock`), allowing a process restricted to site A to host content from site B.
*   **Navigation/Redirect Flaws:** Incorrect handling of security context (origin, isolation state, policies) during navigations, redirects, or history traversals (`NavigationRequest`).
    *   **VRP Relevance:** State confusion after redirects or crashes leading to origin/URL spoofing (VRP: `40057561`, `40064170`; VRP2.txt#9502). Bypasses via same-site redirects (VRP: 150). Leaks via history state (`window.length` VRP: `40059056`, `baseURI` VRP2.txt#15203).
*   **IPC/Mojo Vulnerabilities:** Flaws in validating messages between processes allowing a compromised renderer (or other process) to perform unauthorized actions in the browser process or another renderer.
    *   **VRP Relevance:** Insufficient validation allowing compromised renderer to control mouse (`StartDragging` VRP2.txt#4), send messages as other extensions (VRP2.txt#11815), or bypass origin checks in APIs (VRP: `1263530`). Browser process mishandling renderer message (VRP2.txt#370).
*   **Capability Leaks/SOP Bypass:** Mechanisms allowing isolated processes to gain unintended access to data or capabilities from other origins.
    *   **VRP Relevance:** Leaks via cache timing/TOCTOU (VRP2.txt#542). Leaks via shared workers with data URLs (VRP2.txt#16560). Leaks via `registerProtocolHandler` bypasses (VRP2.txt#12036). SOP bypass via Portal activation (VRP2.txt#8707).
*   **Subsystem Interaction Bugs:** Flaws arising from the interaction of site isolation logic with other features (Service Workers, Extensions, WebUI, Special Frame types, Cache, etc.).
    *   **VRP Relevance:** Service Worker interactions bypassing SameSite/CSP (VRP: `1115438`, `598077`). Extension debugger API bypasses (numerous VRPs, see [extensions_debugger_api.md](extensions_debugger_api.md)). Fenced Frames navigating to `file://` (VRP2.txt#225).

## 3. Known Bypass Techniques and Patterns (from VRP Data)
Analysis of past vulnerabilities reveals recurring patterns used to bypass Site Isolation and Same-Origin Policy protections:

*   **URL Scheme Abuse (`blob:`, `filesystem:`, `data:`, `about:srcdoc`, `javascript:`)**:
    *   Incorrect origin inheritance or handling for these schemes leading to violations (VRP: `40059251`; VRP2.txt#173, #542, #665, #1508, #1690, #1761, #12127). Example: `blob:` URLs created from `data:` URLs inherit opaque origins treated as same-site (VRP2.txt#12127). `about:srcdoc` session history entries could leak state cross-origin (VRP2.txt#1690). Blob URL origin incorrect after tab restore (VRP2.txt#11669).
    *   Using these schemes to bypass CSP or other security checks enforced on standard HTTP(S) origins (VRP2.txt#5009, #11413). Example: `javascript:` URLs in portals or iframes executing in the wrong context (VRP2.txt#1508).
*   **Navigation and Redirect Handling**:
    *   Origin confusion during navigations, especially involving redirects, `javascript:` URIs, or specific URL schemes like `android-app:` (VRP2.txt#173).
    *   Bypasses via open redirects or XSS on same-origin/same-site pages, leveraged by cross-origin frames (VRP: 150).
    *   Race conditions or state mismatches during navigation lifecycle (e.g., race conditions between origin/URL updates and permission checks by DevTools - VRP2.txt#67; URL spoof after crash/failed nav - VRP: `40057561`, `40064170`, VRP2.txt#9502).
    *   Issues with NavigationEntry management or reuse leading to state confusion (VRP2.txt#173, #1690; VRP2.txt#16678 - subframe state corruption).
*   **Cache Timing / TOCTOU**:
    *   Leaking information based on the timing of cache operations (e.g., `GeneratedCodeCache`) due to checks happening before or after potentially attacker-influenced data modification (VRP2.txt#542 - double fetch).
*   **IPC/Mojo Message Handling**:
    *   Insufficient validation of IPC messages, allowing compromised renderers to trigger actions they shouldn't (e.g., sending messages as if from an extension VRP2.txt#11815, bypassing origin checks for service worker/Content Index/Push Messaging interactions - VRP: `1263530`, `1263528`, `1275626`). Mishandling renderer messages in browser (VRP2.txt#370). Mouse control (`StartDragging` VRP2.txt#4).
*   **Special Frames (iframe, portal, fencedframe)**:
    *   Incorrect sandbox inheritance or enforcement, especially involving popups (`allow-popups-to-escape-sandbox` bypasses - VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992).
    *   Issues where frame types like Fenced Frames lack necessary browser-process checks or filtering, allowing navigation to restricted schemes like `file://` (VRP2.txt#225).
    *   Exploiting `about:blank` document reuse logic in sandboxed iframes leading to type system confusion/sandbox escape (VRP2.txt#6788).
    *   Portal activation allowing drag-and-drop SOP bypass (VRP2.txt#8707).
*   **Service Worker Interaction**:
    *   Improperly validating origins or allowing cross-origin fetch interception/manipulation that bypasses SameSite cookies or other restrictions (VRP: `1115438`, VRP2.txt#7521, #11875).
*   **Specific API Misuse**:
    *   Reader Mode loading untrusted content or having weak sanitization, potentially leading to JS execution in privileged context (VRP2.txt#6759).
    *   `registerProtocolHandler` allowing registration of arbitrary schemes/URLs from compromised renderers (VRP2.txt#12036).
    *   Fetch API leaking cross-origin objects (VRP2.txt#6873).

## 4. Key Components Involved
The following components are crucial for implementing site isolation, and each presents potential areas for security logic flaws:

*   **`UrlInfo`**: Packages a `GURL` with state for process allocation decisions. Incorrect state can lead to wrong isolation. See [url_info.md](url_info.md).
*   **`SiteInfo`**: Represents the "site" of a URL. Logic errors can cause incorrect same-site/cross-site classification. See [site_info.md](site_info.md).
*   **`SiteInstanceImpl`**: Represents an instance of a site, managing the associated process. Errors in process assignment, reuse checks (`MayReuseAndIsSuitable`), or locking (`SetProcessInternal`, `LockProcessIfNeeded`) are critical. See [site_instance.md](site_instance.md).
*   **`SiteInstanceGroup`**: Groups `SiteInstance`s sharing a `RenderProcessHost`. Errors in lifecycle management or `SiteInstance` association can break isolation. See [site_instance_group.md](site_instance_group.md).
*   **`BrowsingInstance`**: Groups `SiteInstance`s sharing a browsing context (e.g., related windows/tabs). Errors can lead to inconsistent isolation across contexts. See [browsing_instance.md](browsing_instance.md).
*   **`NavigationRequest`**: Manages navigation lifecycle, determines `UrlInfo`, enforces policies. Logic errors here directly impact isolation decisions. See [navigation_request.md](navigation_request.md).
*   **`RenderProcessHostImpl`**: Browser-side representation of a renderer process. Enforces process locks, manages lifecycle, handles communication. Errors in reuse logic (`MayReuseAndIsSuitable`) or locking (`SetProcessLock`) are critical. See [render_process_host.md](render_process_host.md).
*   **`ChildProcessSecurityPolicyImpl`**: Manages security policies for child processes (permissions, URL access). Errors in granting/checking permissions (`CanRequestURL`, `CanRedirectToURL`) can lead to unauthorized access, especially for special schemes. See [child_process_security_policy_impl.md](child_process_security_policy_impl.md).
*   **[IPC](ipc.md) / [Mojo](mojo.md)**: Inter-process communication. Flaws in message validation (origin checks, type confusion) are prime targets for compromised renderers.

## 5. Areas Requiring Further Investigation
*   Interaction between `UrlInfo::GetPossiblyOverriddenOriginFromUrl` and unique/opaque origins (`blob:`, `filesystem:`, `data:`).
*   Process lock URL determination for WebUI.
*   Robustness of `RenderProcessHostImpl::MayReuseAndIsSuitable` against all site/origin/policy combinations.
*   Interaction between site isolation and other mechanisms (CSP, CORS, Sandboxing, Service Workers, Extensions, BFCache).
*   `ChildProcessSecurityPolicyImpl` handling of special schemes and permissions.
*   `SiteInstanceImpl` logic for process locking (`LockProcessIfNeeded`) and reuse (`ReuseExistingProcessIfPossible`, `IsSuitableForUrlInfo`).
*   `SiteInstanceGroup` lifecycle management.

## 6. Related VRP Reports (Illustrative)
*   VRP: `40059251` (javascript/data origin confusion)
*   VRP: `40053875` (WebUI MojoJS binding issue)
*   VRP: `40059056` (`window.length` leak)
*   VRP: `40057561` / VRP2.txt#9502 (URL Spoof after crash)
*   VRP: 150 (Top-level navigation bypass via same-site redirect)
*   VRP2.txt#542 (Cache double fetch timing)
*   VRP2.txt#6261 (Filesystem access)
*   VRP2.txt#173 (javascript/data origin confusion)
*   VRP2.txt#370 (IPC message handling)
*   VRP2.txt#11815 (Extension IPC spoofing)
*   VRP2.txt#7849 (`allow-popups-to-escape-sandbox` bypass)
*   VRP2.txt#6788 (Sandbox context reuse)
*   VRP2.txt#8707 (Portal activation drag/drop bypass)
*   VRP2.txt#7521 (Service Worker SameSite bypass)
*   VRP2.txt#12036 (`registerProtocolHandler` bypass)
*   VRP2.txt#6873 (Fetch object leak)

*(This list is illustrative; many VRPs touch on isolation principles. Refer to specific component pages and VRP files for more context.)*

## 7. Secure Contexts and Site Isolation
Site isolation is fundamental to maintaining secure context guarantees. By isolating sites, it prevents scripts from insecure contexts (e.g., HTTP) from interacting with secure contexts (e.g., HTTPS), mitigating mixed content issues and other attacks that rely on breaching origin boundaries.

## 8. Privacy Implications
Site isolation enhances privacy by preventing cross-site tracking through direct script interaction or shared process resources. However, care must be taken to ensure that the isolation mechanisms themselves don't introduce new privacy-leaking side channels (e.g., via process allocation timing, cache behavior - VRP2.txt#542). Storage partitioning, managed partly through `RenderProcessHostImpl`, is also crucial for privacy.

*(See also: [ipc.md](ipc.md), [mojo.md](mojo.md), [navigation_request.md](navigation_request.md), [service_workers.md](service_workers.md), [worker_threads.md](worker_threads.md), [fenced_frames.md](fenced_frames.md), [portals.md](portals.md))*
