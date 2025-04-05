# Site Isolation: A Focus on Finding Security Logic Issues

This document provides a high-level overview of site isolation in the Chromium project, with a particular focus on identifying potential security logic issues. For more detailed information about specific components, please refer to the linked pages.

## Core Concepts

Site isolation is a critical security mechanism in Chromium that aims to isolate web content from different sites into separate processes. This prevents malicious websites from accessing data or resources from other sites, even if they are running in the same browser instance. A failure in the logic that determines site isolation can lead to severe security vulnerabilities, such as cross-site scripting (XSS) and data leakage.

## Key Components and Potential Security Issues

The following components are crucial for implementing site isolation, and each presents potential areas for security logic flaws:

*   **`UrlInfo`**: This struct packages a `GURL` together with extra state required to make `SiteInstance`/process allocation decisions. Incorrectly populating or interpreting this state can lead to incorrect isolation decisions. See [UrlInfo](url_info.md) for more details. **Specific research areas:** Investigate how different URL schemes (e.g., `blob:`, `filesystem:`, `data:`, `about:srcdoc`) are handled and if they can lead to origin confusion (VRP2.txt Line 12127: `blob:null` issue; VRP2.txt Line 1690: `about:srcdoc` history leak).
*   **`SiteInfo`**: This class represents the site of a URL and determines if two URLs belong to the same site. Logic errors here can lead to incorrect same-site/cross-site classifications, potentially allowing cross-site attacks. See [SiteInfo](site_info.md) for more details. **Specific research areas:** Focus on edge cases in site determination, especially involving subdomains, ports, IP addresses, and effective URLs (`SiteInfo::GetSiteForURLInternal`).
*   **`SiteInstanceImpl`**: This class represents an instance of a site and is responsible for managing the associated process. Incorrectly assigning a process or handling the site URL can lead to vulnerabilities. See [SiteInstance](site_instance.md) for more details. **Specific research areas:** Examine how `SiteInstanceImpl` handles navigations to different origins (`IsNavigationSameSite`, `ConvertToDefaultOrSetSite`), how it ensures that the process is correctly locked (`SetProcessInternal`, `LockProcessIfNeeded`), and how process reuse (`ReuseExistingProcessIfPossible`, `IsSuitableForUrlInfo`) interacts with isolation. Check how `SetSiteInfoInternal` initialization affects security state.
*   **`SiteInstanceGroup`**: This class represents a group of `SiteInstance` objects that share the same `RenderProcessHost`. Errors in managing the lifecycle (`RenderProcessHostDestroyed`, `RenderProcessExited`) of the `RenderProcessHost` or associating `SiteInstance` objects can lead to security issues. See [SiteInstanceGroup](site_instance_group.md) for more details. **Specific research areas:** Investigate how `SiteInstanceGroup` manages process reuse (`RenderProcessHostImpl::MayReuseAndIsSuitable`) and how it prevents cross-site data leakage when processes are reused. Ensure robust lifetime management via `base::SafeRef`/`base::WeakPtr`.
*   **`BrowsingInstance`**: This class represents a group of `SiteInstance`s that share the same browsing context. Errors in managing `BrowsingInstance`s can lead to inconsistent isolation decisions across tabs and windows. See [BrowsingInstance](browsing_instance.md) for more details.
*   **`NavigationRequest`**: This class manages the navigation lifecycle and determines the `UrlInfo` for the navigation. Logic errors here can result in incorrect isolation decisions for navigations, potentially leading to security breaches. See [NavigationRequest](navigation_request.md) for more details. **Specific research areas:** Analyze how `NavigationRequest` handles redirects, especially those involving different schemes or origins (VRP: 150; VRP2.txt Line 173).
*   **`RenderProcessHostImpl`**: This class represents the browser side of the browser <--> renderer communication channel. It is responsible for managing the lifecycle (`Init`, `Shutdown`, `FastShutdownIfPossible`, `OnProcessLaunched`, `OnChannelError`, `OnBadMessageReceived`) of the renderer process and for enforcing security policies. Incorrectly determining if a process can be reused, mismanaging the process lifecycle, or making incorrect priority assumptions (`UpdateProcessPriority`) can lead to vulnerabilities. See [RenderProcessHost](render_process_host.md) for more details. **Specific research areas:** Examine how `RenderProcessHostImpl` enforces the Same Origin Policy, process locks (`SetProcessLock`), and prevents unauthorized access to resources.
*   **`ChildProcessSecurityPolicyImpl`**: This class manages the security policy for child processes, including granting and revoking permissions. Incorrectly granting permissions or errors in checking permissions (`CanRequestURL`) can lead to unauthorized access to resources, especially involving special schemes (VRP2: 6261 - filesystem). See [ChildProcessSecurityPolicyImpl](child_process_security_policy_impl.md) for more details. **Specific research areas:** Investigate how `ChildProcessSecurityPolicyImpl` handles file access and other sensitive permissions, and if there are any ways to bypass these checks for schemes like `blob:` or `filesystem:`.
*   **[IPC](ipc.md) / [Mojo](mojo.md)**: Inter-process communication mechanisms. Flaws in message validation (e.g., missing origin checks, incorrect handling of messages from compromised renderers - VRP2.txt Line 370, 11815) can lead to isolation bypasses or unauthorized actions.

## Known Bypass Techniques and Patterns (from VRP Data)

Analysis of past vulnerabilities reveals recurring patterns used to bypass Site Isolation and Same-Origin Policy protections:

*   **URL Scheme Abuse (`blob:`, `filesystem:`, `data:`, `about:srcdoc`, `javascript:`)**:
    *   Incorrect origin inheritance or handling for these schemes leading to violations (VRP: `40059251`; VRP2.txt Line 173, 542, 665, 1508, 1688, 1761). Example: `blob:` URLs created from `data:` URLs inherit opaque origins treated as same-site (VRP2.txt Line 12127). `about:srcdoc` session history entries could leak state cross-origin (VRP2.txt Line 1690). Blob URL origin incorrect after tab restore (VRP2.txt Line 11669). iOS blob URL origin display issue (VRP2.txt Line 11681).
    *   Using these schemes to bypass CSP or other security checks enforced on standard HTTP(S) origins (VRP2.txt Line 5009, 5049, 11413). Example: `javascript:` URLs in portals or iframes executing in the wrong context (VRP2.txt Line 1508), or bypassing origin confusion mitigations (VRP: `40059251`).
*   **Navigation and Redirect Handling**:
    *   Origin confusion during navigations, especially involving redirects, `javascript:` URIs, or specific URL schemes like `android-app:` (VRP2.txt Line 173).
    *   Bypasses via open redirects or XSS on same-origin/same-site pages, leveraged by cross-origin frames (VRP: 150).
    *   Race conditions or state mismatches during navigation lifecycle (e.g., race conditions between origin/URL updates and permission checks by DevTools - VRP2.txt Line 67; URL spoof after crash/failed nav - VRP: `40057561`, VRP2.txt Line 9502).
    *   Issues with NavigationEntry management or reuse leading to state confusion (VRP2: 173, 1690; VRP2.txt Line 16678).
*   **Cache Timing / TOCTOU**:
    *   Leaking information based on the timing of cache operations (e.g., `GeneratedCodeCache`) due to checks happening before or after potentially attacker-influenced data modification (VRP2.txt Line 542).
*   **IPC/Mojo Message Handling**:
    *   Insufficient validation of IPC messages, allowing compromised renderers to trigger actions they shouldn't (e.g., sending messages as if from an extension VRP2.txt Line 11815, bypassing origin checks for service worker/Content Index/Push Messaging interactions - VRP: `1263530`, `1263528`, `1275626`). (VRP2.txt Line 370).
*   **Special Frames (iframe, portal, fencedframe)**:
    *   Incorrect sandbox inheritance or enforcement, especially involving popups (`allow-popups-to-escape-sandbox` bypasses - VRP: `40069622`, `40057525`; VRP2.txt Line 7849, 11992) or navigations (VRP2.txt Line 6788). Sandboxed frames downloading despite restrictions (VRP2.txt Line 11682).
    *   Issues where frame types like Fenced Frames lack necessary browser-process checks or filtering, allowing navigation to restricted schemes like `file://` (VRP2.txt Line 225). Fenced frame two-way communication side-channel (VRP2.txt Line 3393).
    *   Exploiting `about:blank` document reuse logic in sandboxed iframes leading to type system confusion/sandbox escape (VRP2.txt Line 6788).
    *   Portal activation allowing drag-and-drop SOP bypass (VRP2.txt Line 8707).
*   **Service Worker Interaction**:
    *   Improperly validating origins or allowing cross-origin fetch interception/manipulation that bypasses SameSite cookies or other restrictions (VRP2.txt Line 7521, 6261, 11875).
*   **Specific API Misuse**:
    *   Reader Mode loading untrusted content or having weak sanitization, potentially leading to JS execution in privileged context (VRP2.txt Line 6759).
    *   `registerProtocolHandler` allowing registration of arbitrary schemes/URLs from compromised renderers (VRP2.txt Line 12036).

## Areas Requiring Further Investigation

The following areas require further investigation, but note that some previously known vulnerabilities have been fixed:

*   The interaction between `GetPossiblyOverriddenOriginFromUrl` and different types of URLs, especially those with unique origins (e.g., blob URLs, filesystem URLs - VRP2: 1761, 6261).
*   The logic for determining the process lock URL for WebUI URLs and how it interacts with the site URL.
*   The impact of incorrect origin handling on cross-origin communication and data access.
*   The security implications of using effective URLs in `SiteInfo::GetSiteForURLInternal`.
*   The logic for determining when a dedicated process is required and when a process can be reused (`RenderProcessHostImpl::MayReuseAndIsSuitable`).
*   The interaction between site isolation and other security mechanisms, such as Content Security Policy (CSP) and Cross-Origin Resource Sharing (CORS).
*   The role of `SiteInstanceGroup` in managing the lifecycle of `RenderProcessHost` objects and how it affects site isolation.
*   How the `RenderProcessHost` is accessed from a `SiteInstance` through the `SiteInstanceGroup`.
*   The logic for granting and revoking permissions in `ChildProcessSecurityPolicyImpl` and how it affects the security of the renderer process (e.g., for `filesystem:` VRP2: 6261).
*   The logic within `SiteInstanceImpl::SetProcessInternal` to ensure that the process is correctly locked to the site.
*   The logic within `SiteInstanceImpl::ReuseExistingProcessIfPossible` to ensure that processes are reused correctly.
*   The logic within `SiteInstanceImpl::IsSuitableForUrlInfo` to ensure that the SiteInstance is suitable for a given URL.
*   The logic within `SiteInstanceImpl::IsNavigationSameSite` to ensure that navigations are correctly classified as same-site or cross-site.
*   The usage of `SiteInstanceImpl::SetSiteInfoInternal` to ensure that all fields are correctly initialized.
*   The usage of `SiteInstanceImpl::ConvertToDefaultOrSetSite` to ensure that SiteInstances are correctly converted to the default SiteInstance or have their site set.
*   The logic within `SiteInstanceImpl::LockProcessIfNeeded` to ensure that the process is correctly locked to the site.
*   The logic within `SiteInstanceGroup::RenderProcessHostDestroyed` and `::RenderProcessExited` to ensure correct cleanup and notification.
*   The usage of `base::SafeRef` and `base::WeakPtr` to manage the lifetime of the `SiteInstanceGroup` and its associated objects.
*   The logic within `RenderProcessHostImpl::Init`, `::Shutdown`, `::FastShutdownIfPossible`, `::OnProcessLaunched`, `::OnChannelError`, `::OnBadMessageReceived`.
*   The logic within `RenderProcessHostImpl::UpdateProcessPriority` and `::SetProcessLock`.

**Note:** The following specific vulnerabilities related to Site Isolation have been fixed according to VRP data:

*   Browser-side origin confusion for javascript/data URLs opened in a new window/tab by cross-origin iframe (VRP: `40059251`)
*   Some WebUI pages enable MojoJS bindings for the subsequently-navigated site (VRP: `40053875`)
*   Leaking window.length without opener reference (VRP: `40059056`)
*   URL Spoof after crash (VRP: `40057561`)
*   Site Isolation break because of double fetch of shared buffer (VRP2.txt Line 542)
*   Site Isolation breaking bug in filesystem (VRP2.txt Line 6261)

## Secure Contexts and Site Isolation

Site isolation is a critical component for maintaining secure contexts. By isolating different sites into separate processes, it prevents malicious websites from accessing data or resources from other sites, even if they are running in the same browser instance. This is particularly important for sensitive data and operations that require a secure context.

## Privacy Implications

Site isolation can also have privacy implications. By isolating sites into separate processes, it can help prevent cross-site tracking and other privacy-related issues. However, it's important to ensure that site isolation is implemented correctly to avoid any unintended privacy leaks or side-channels (e.g., timing attacks related to cache or process management).
