# Component: Blink > SecurityFeature > IFrameSandbox

## 1. Component Focus
*   **Functionality:** Implements the `sandbox` attribute for `<iframe>` elements ([Spec](https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox)), restricting the capabilities of the embedded content. Also interacts with the `sandbox` directive in Content Security Policy (CSP).
*   **Key Logic:** Parsing the `sandbox` attribute (`HTMLIFrameElement::ParseSandboxAttribute`), applying CSP sandbox directives (`DocumentLoader::InitializeWindow` -> `CreateCSP` -> `LocalDOMWindow::SetContentSecurityPolicy`), combining attribute flags with CSP sandbox flags (`NavigationPolicyContainerBuilder::ComputeSandboxFlags` before commit), inheriting flags to nested frames/popups (partly in Blink's `CreateNewWindow` for initial calculation, browser's `RenderFrameHostImpl::InitializePolicyContainerHost` for application to initial empty doc, and `FrameTree::FindOrCreateFrameForNavigation` for routing), enforcing restrictions during navigation (`NavigationRequest`, `FrameLoader`), popup creation (`LocalDOMWindow::open`), script execution, form submission, downloads, etc.
*   **Core Files:**
    *   `third_party/blink/renderer/core/frame/local_dom_window.cc` (Implements `window.open`)
    *   `third_party/blink/renderer/core/page/create_window.cc` (Helper for `window.open`, contains Blink's `CreateNewWindow`)
    *   `third_party/blink/renderer/core/page/frame_tree.cc` (Handles `FindOrCreateFrameForNavigation`)
    *   `third_party/blink/renderer/core/html/html_iframe_element.cc` (Parsing attribute)
    *   `third_party/blink/renderer/core/frame/sandbox_flags.h`
    *   `third_party/blink/renderer/core/loader/document_loader.cc` (Applies CSP during commit)
    *   `third_party/blink/renderer/core/frame/csp/content_security_policy.cc` (Handles CSP parsing/application)
    *   `content/browser/renderer_host/render_frame_host_impl.cc` (Contains browser-side `CreateNewWindow` and `InitializePolicyContainerHost`)
    *   `content/browser/renderer_host/navigation_policy_container_builder.cc` (Computes final flags before commit)
    *   `content/browser/renderer_host/navigation_request.cc` (Uses final flags)
    *   `third_party/blink/renderer/core/loader/frame_loader.cc`

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing Specific Sandbox Flags:** Finding ways to perform actions restricted by flags like `allow-scripts`, `allow-popups`, `allow-top-navigation`, `allow-downloads`, `allow-same-origin`. This often stems from incorrect flag calculation/inheritance *before* the final commit, or specific interactions.
    *   **VRP Pattern (`allow-popups-to-escape-sandbox` Bypass):** Popups opened from sandboxed frames incorrectly inheriting opener's capabilities or losing restrictions.
        *   **Analysis:**
            1.  Blink's `CreateNewWindow` determines *initial* popup sandbox flags based on the opener's state (`HTMLIFrameElement` attribute + *currently active* CSP sandbox flags) and `allow-popups-to-escape-sandbox`.
            2.  These initial flags are passed to the browser.
            3.  Browser's `RenderFrameHostImpl::InitializePolicyContainerHost` applies these initial flags (combined with opener's CSP sandbox flags from *its* loaded policy container) to the popup's *initial empty document*.
            4.  Blink's `DocumentLoader::InitializeWindow` applies the *committed document's* CSP sandbox directive when the navigation commits within the iframe.
        *   **Hypothesis (Refined):** Bypasses likely occur because the opener frame navigates to a same-origin document (Doc B) *before* calling `window.open`. If Doc B's CSP lacks a restrictive `sandbox` directive, its commit (`DocumentLoader::InitializeWindow`) updates the frame's *active* sandbox flags in its `SecurityContext`. *Then*, when `window.open` is called, Blink's `CreateNewWindow` reads these *updated* (less restrictive) flags, leading to the popup inheriting incorrect permissions. (VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992).
    *   **VRP Pattern (`allow-downloads` Bypass):** Triggering downloads from a frame sandboxed without `allow-downloads`. (VRP: `40060695`, VRP2.txt#11682). Interaction with `window.open` + `noopener` (VRP2.txt#1105). **Checks potentially missing in specific download paths (e.g., involving `noopener`).**
    *   **VRP Pattern (`allow-top-navigation` Bypass):** Navigating the top-level frame from a cross-origin iframe without user interaction, even when only `allow-scripts` and `allow-popups` (or similar combinations) are set, but `allow-top-navigation*` is *not*.
        *   Via same-site open redirects or XSS redirects (VRP: `40053936`). **Likely navigation checks failing to account for sandbox post-redirect.**
        *   Via `Content-Security-Policy: sandbox allow-top-navigation` header on the *iframe's response* (VRP2.txt#4247, #8387 - bypass of fix for crbug/1145553). **CSP correctly adds the flag during `ComputeSandboxFlags`, but initial navigation checks might not anticipate this.**
        *   Via `onkeydown`/`onblur` event handlers (VRP2.txt#8121). **Timing/event handling issue.**
*   **Incorrect Flag Inheritance/Parsing:** Errors in `HTMLIFrameElement::ParseSandboxAttribute` or initial inheritance logic *before* final computation/commit.
*   **Interaction with Schemes/APIs:** Exploiting interactions between the sandbox and specific URL schemes or browser features.
    *   **VRP Pattern (`intent://` Bypass - Android):** Using crafted `intent://` URLs, possibly nested within `data:` URIs or triggered via popups, to bypass sandbox restrictions (e.g., trigger downloads, navigate top). **Intent handling likely doesn't correctly inherit/check sandbox flags.** (VRP: `1365100`; VRP2.txt#7507, #10199, #15706).
    *   **VRP Pattern (`javascript:` Bypass):** Using `javascript:` links opened in new tabs (`target="_blank"`) combined with `window.opener` to execute script in the opener's context, bypassing `allow-scripts` if `allow-popups-to-escape-sandbox` is present. **Issue with how `window.opener` context and sandbox interact.** (VRP: `1017879`; VRP2.txt#11380).
*   **Origin/Context Confusion:** Sandboxed frames (especially `allow-same-origin`) potentially ending up in unexpected processes or sharing execution contexts improperly.
    *   **VRP Pattern (Context Reuse):** Sandboxed `allow-same-origin` iframe navigated from initial `about:blank` reusing the non-sandboxed context, leading to type confusion/sandbox escape (VRP2.txt#6788).

## 3. Further Analysis and Potential Issues
*   **Flag Combinations:** Synergistic effects?
*   **Inheritance Logic:** Focus on **intermediate navigation scenarios** within the sandboxed frame *before* popup creation, specifically how the timing of CSP application (`DocumentLoader::InitializeWindow`) interacts with `window.open` calls (`CreateNewWindow`).
*   **`allow-same-origin`:** Request handling, process allocation.
*   **Parsing Robustness.**
*   **Interaction with Navigation Logic:** Enforcement of *final* computed flags at all stages, especially after redirects or CSP application.
*   **Platform Differences (Android Intents).**

## 4. Code Analysis
*   `HTMLIFrameElement::ParseSandboxAttribute` (Blink): Parses `sandbox` attribute string, sets initial flags.
*   `DocumentLoader::InitializeWindow` (Blink): Called during commit. Creates/Applies `ContentSecurityPolicy` (`CreateCSP`, `SetContentSecurityPolicy`), which updates active sandbox flags via `ContentSecurityPolicy::ApplySandboxFlags`.
*   `LocalDOMWindow::open` (Blink): Calls `FrameTree::FindOrCreateFrameForNavigation`.
*   `FrameTree::FindOrCreateFrameForNavigation` (Blink): Finds existing frame or calls Blink's `CreateNewWindow`.
*   `CreateNewWindow` (`create_window.cc`, Blink): Reads opener's *current* sandbox flags (`IsSandboxed`, `GetSandboxFlags`) and `allow-popups-to-escape-sandbox` to determine *initial* popup flags. Calls `ChromeClient::CreateWindow`.
*   `ChromeClientImpl::CreateWindowDelegate` (Blink): Bridge, passes initial flags to browser.
*   `RenderFrameHostImpl::CreateNewWindow` (Browser): Delegates window creation.
*   `RenderFrameHostImpl::InitializePolicyContainerHost` (Browser): Sets up popup's initial empty document policies, cloning opener's policies and combining initial flags from Blink with opener's CSP sandbox flags.
*   `NavigationPolicyContainerBuilder::ComputeSandboxFlags` (Browser): Computes *final* flags before commit, combining iframe attributes, CSP headers, etc.
*   `Frame::IsSandboxed(flag)` (Blink): Checks currently active flags in `SecurityContext`.
*   `FrameLoader::CanNavigate` (Blink): Checks `allow-top-navigation*`.

## 5. Areas Requiring Further Investigation
*   **Verify intermediate navigation timing:** Confirm exact point during `DocumentLoader::CommitNavigation` where `InitializeWindow` (and CSP sandbox application) occurs relative to script execution.
*   **Browser-Side `RenderFrameHostDelegate::CreateNewWindow`:** Analyze `WebContentsImpl` implementation details.
*   **Android `intent://` handling:** Complete flow analysis.
*   **`allow-top-navigation` enforcement:** vs. CSP header timing.
*   **`allow-downloads` enforcement:** vs. `noopener`.
*   **`allow-same-origin` isolation:** Context/process management.

## 6. Related VRP Reports
*   **`allow-popups-to-escape-sandbox` Bypass:** VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992
*   **`allow-downloads` Bypass:** VRP: `40060695`; VRP2.txt#11682, #1105
*   **`allow-top-navigation` Bypass:** VRP: `40053936` (via redirect); VRP2.txt#4247, #8387 (via CSP header), VRP2.txt#8121 (via events)
*   **Intent Scheme Bypass (Android):** VRP: `1365100`; VRP2.txt#7507, #10199, #15706
*   **`javascript:` URI Bypass:** VRP: `1017879`; VRP2.txt#11380
*   **Context Reuse/Type Confusion (`allow-same-origin`):** VRP2.txt#6788

*(Note: Sandbox bypasses often interact with other features like Navigation, CSP, or platform-specific mechanisms like Android Intents)*