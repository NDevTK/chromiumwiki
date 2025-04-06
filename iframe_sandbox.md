# Component: Blink > SecurityFeature > IFrameSandbox

## 1. Component Focus
*   **Functionality:** Implements the `sandbox` attribute for `<iframe>` elements ([Spec](https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox)), restricting the capabilities of the embedded content.
*   **Key Logic:** Parsing the `sandbox` attribute (`SandboxFlags ParseSandboxPolicy`), enforcing restrictions during navigation (`NavigationRequest`, `FrameLoader`), popup creation (`window.open`), script execution, form submission, downloads, etc. Inheritance of flags to nested frames and popups.
*   **Core Files:**
    *   `third_party/blink/renderer/core/html/html_iframe_element.cc`
    *   `third_party/blink/renderer/core/frame/sandbox_flags.h`
    *   `third_party/blink/renderer/core/frame/frame.cc` (checks like `IsSandboxed`)
    *   `content/browser/renderer_host/navigation_request.cc` (checks sandbox flags during navigation)
    *   `third_party/blink/renderer/core/loader/frame_loader.cc`

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing Specific Sandbox Flags:** Finding ways to perform actions restricted by flags like `allow-scripts`, `allow-popups`, `allow-top-navigation`, `allow-downloads`, `allow-same-origin`.
    *   **VRP Pattern (`allow-popups-to-escape-sandbox` Bypass):** Techniques allowing popups opened from a sandboxed frame (without `allow-popups-to-escape-sandbox`) to gain capabilities of the top-level context or escape the sandbox restrictions. (VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992). Often involves specific timing or intermediate navigations (e.g., Android `SignOutOptions` VRP2.txt#7849). Blank iframe intermediate step (VRP2.txt#11992).
    *   **VRP Pattern (`allow-downloads` Bypass):** Triggering downloads from a frame sandboxed without `allow-downloads`. (VRP: `40060695`, VRP2.txt#11682). Interaction with `window.open` + `noopener` (VRP2.txt#1105).
    *   **VRP Pattern (`allow-top-navigation` Bypass):** Navigating the top-level frame from a cross-origin iframe without user interaction, even when only `allow-scripts` and `allow-popups` (or similar combinations) are set, but `allow-top-navigation*` is *not*.
        *   Via same-site open redirects or XSS redirects (VRP: `40053936`).
        *   Via `Content-Security-Policy: sandbox allow-top-navigation` header on the *iframe's response* (VRP2.txt#4247, #8387 - bypass of fix for crbug/1145553).
        *   Via `onkeydown`/`onblur` event handlers (VRP2.txt#8121).
*   **Incorrect Flag Inheritance/Parsing:** Errors in how sandbox flags are parsed from the attribute string or inherited by nested frames or popups.
*   **Interaction with Schemes/APIs:** Exploiting interactions between the sandbox and specific URL schemes or browser features.
    *   **VRP Pattern (`intent://` Bypass - Android):** Using crafted `intent://` URLs, possibly nested within `data:` URIs or triggered via popups, to bypass sandbox restrictions (e.g., trigger downloads, navigate top). (VRP: `1365100`; VRP2.txt#7507, #10199, #15706).
    *   **VRP Pattern (`javascript:` Bypass):** Using `javascript:` links opened in new tabs (`target="_blank"`) combined with `window.opener` to execute script in the opener's context, bypassing `allow-scripts` if `allow-popups-to-escape-sandbox` is present. (VRP: `1017879`; VRP2.txt#11380).
*   **Origin/Context Confusion:** Sandboxed frames (especially `allow-same-origin`) potentially ending up in unexpected processes or sharing execution contexts improperly.
    *   **VRP Pattern (Context Reuse):** Sandboxed `allow-same-origin` iframe navigated from initial `about:blank` reusing the non-sandboxed context, leading to type confusion/sandbox escape (VRP2.txt#6788).

## 3. Further Analysis and Potential Issues
*   **Flag Combinations:** How do different combinations of sandbox flags interact? Are there unexpected synergistic effects? (e.g., `allow-popups` + `allow-scripts` + `allow-same-origin`).
*   **Inheritance Logic:** Deep dive into how flags are inherited by nested iframes and popups (`window.open`). Are flags correctly propagated in all scenarios (e.g., redirects within the sandboxed frame before opening popup)?
*   **`allow-same-origin`:** This flag is particularly dangerous. How are requests and process allocation handled when this is set? Does it correctly maintain an opaque origin for security checks while allowing DOM access? (VRP2.txt#6788 highlights issues).
*   **Parsing Robustness:** Is the `SandboxFlags ParseSandboxPolicy` function robust against malformed or unusual attribute values?
*   **Interaction with Navigation Logic:** How does `NavigationRequest` check and enforce sandbox flags at different stages (start, redirect, commit)? Are there potential races or bypasses?
*   **Platform Differences (Android Intents):** The interaction with Android Intents seems particularly prone to bypasses (VRP: `1365100`). Requires specific attention to how intents launched from sandboxed contexts are handled.

## 4. Code Analysis
*   `HTMLIFrameElement::ParseSandboxAttribute`: Parses the `sandbox` attribute string into flags.
*   `SandboxFlags` enum (`core/frame/sandbox_flags.h`): Defines the different sandbox flags.
*   `Frame::IsSandboxed(SandboxFlags)`: Checks if specific sandbox flags are active for the frame. Used throughout Blink and Content layers.
*   `NavigationRequest::CheckSandboxFlags`: Checks sandbox flags during navigation commit.
*   `FrameLoader::CanNavigate`: Checks sandbox flags related to navigation (`allow-top-navigation`, `allow-popups-to-escape-sandbox`).
*   `WindowOrWorkerGlobalScope::Open`: Logic for `window.open`, including sandbox inheritance for popups.
*   `DownloadManagerImpl` / `DownloadRequestLimiter`: Potentially involved in enforcing `allow-downloads`.
*   Android Intent handling code (`ExternalNavigationHandler.java`?): Check how intents originating from sandboxed frames are processed.

## 5. Areas Requiring Further Investigation
*   **Thorough review of popup sandbox inheritance:** Ensure flags are correctly and consistently inherited, especially with intermediate navigations or complex `window.open` parameters (`noopener`). (VRP: `40069622`, `40057525`).
*   **Android `intent://` handling:** Analyze the complete flow of handling `intent://` URLs initiated from sandboxed frames, including interactions with the Android system and potential lack of sandbox enforcement in intent receivers (VRP: `1365100`).
*   **`allow-top-navigation` enforcement:** Re-evaluate all mechanisms that could trigger top-level navigation from an iframe (direct `top.location` assignment, form submissions, link clicks, script execution) and ensure the `allow-top-navigation*` flags are correctly checked *before* the navigation proceeds, including cases involving CSP headers (VRP2.txt#4247, #8387) or event handlers (VRP2.txt#8121).
*   **`allow-downloads` enforcement:** Investigate how downloads triggered via `window.open` + `noopener` bypass the restriction (VRP2.txt#1105). Ensure all download initiation paths respect the sandbox flag.
*   **`allow-same-origin` isolation:** Audit the context/process management for frames with `allow-same-origin` to prevent type system confusion or unintended capability sharing (VRP2.txt#6788).

## 6. Related VRP Reports
*   **`allow-popups-to-escape-sandbox` Bypass:** VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992
*   **`allow-downloads` Bypass:** VRP: `40060695`; VRP2.txt#11682, #1105
*   **`allow-top-navigation` Bypass:** VRP: `40053936` (via redirect); VRP2.txt#4247, #8387 (via CSP header), VRP2.txt#8121 (via events)
*   **Intent Scheme Bypass (Android):** VRP: `1365100`; VRP2.txt#7507, #10199, #15706
*   **`javascript:` URI Bypass:** VRP: `1017879`; VRP2.txt#11380
*   **Context Reuse/Type Confusion (`allow-same-origin`):** VRP2.txt#6788

*(Note: Sandbox bypasses often interact with other features like Navigation, CSP, or platform-specific mechanisms like Android Intents)*