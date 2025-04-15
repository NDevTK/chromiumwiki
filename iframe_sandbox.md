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
*   **Bypassing Specific Sandbox Flags:** Finding ways to perform actions restricted by flags like `allow-scripts`, `allow-popups`, `allow-top-navigation`, `allow-downloads`, `allow-same-origin`. This often stems from incorrect flag calculation/inheritance *before* the final commit, or specific interactions with other features or schemes.
    *   **VRP Pattern (`allow-popups-to-escape-sandbox` Bypass):** Popups opened from sandboxed frames incorrectly inheriting opener's capabilities or losing restrictions. This can happen when the opener frame navigates to a same-origin document with a less restrictive or missing CSP `sandbox` directive *before* calling `window.open`. Blink's `CreateNewWindow` reads the opener's *updated* flags, leading to the popup inheriting incorrect permissions.
        *   _Examples:_ VRP: `40069622`, `40057525`; VRP2.txt#7849 (Android SignOutOptions), #11992 (blank iframe `window.open`), #212 (general bypass), #12191 (blank iframe `window.open` variation).
    *   **VRP Pattern (`allow-downloads` Bypass):** Triggering downloads from a frame sandboxed without `allow-downloads`. Often involves interaction with `window.open`.
        *   _Examples:_ VRP: `40060695` (Opening `.SettingContent-ms` file download which executes via OS handler); VRP2.txt#11682 (Drag-and-drop filter bypass leading to RCE via allowed handler for `file://` download), #1105 (Interaction with `window.open` + `noopener`).
    *   **VRP Pattern (`allow-top-navigation` Bypass):** Navigating the top-level frame from a cross-origin iframe without user interaction, even when only `allow-scripts` and `allow-popups` (or similar combinations) are set, but `allow-top-navigation*` is *not*.
        *   _Mechanism 1: Redirects:_ Via same-site open redirects or XSS redirects. Navigation checks likely fail to re-evaluate sandbox flags after the redirect. (VRP: `40053936`).
        *   _Mechanism 2: CSP Header:_ Via `Content-Security-Policy: sandbox allow-top-navigation` header on the *iframe's response*. The CSP correctly adds the flag during `ComputeSandboxFlags`, but the initial navigation check might occur before this final computation, allowing the navigation. (VRP2.txt#4247, #8387 - bypass of fix for crbug/1145553).
        *   _Mechanism 3: Event Handling:_ Via `onkeydown`/`onblur` event handlers. Timing/event handling likely allows bypassing checks. (VRP2.txt#8121).
*   **Interaction with Schemes/APIs:** Exploiting interactions between the sandbox and specific URL schemes or browser features.
    *   **VRP Pattern (`intent://` Bypass - Android):** Using crafted `intent://` URLs, possibly nested within `data:` URIs or triggered via popups (`allow-popups` required), to bypass sandbox restrictions (e.g., trigger downloads, navigate top). Intent handling likely doesn't correctly inherit/check sandbox flags, or specific components (like Firebase Dynamic Links) facilitate the bypass.
        *   _Examples:_ VRP: `1365100`; VRP2.txt#7507, #10199 (SameSite bypass), #15706, #15724 (FDL bypass), #8642 (FDL bypass).
    *   **VRP Pattern (`javascript:` URI Bypass):** Using `javascript:` links opened in new tabs/windows (`target="_blank"`) combined with `window.opener` to execute script in the opener's context, bypassing `allow-scripts` if `allow-popups-to-escape-sandbox` is present. Issue with how `window.opener` context and sandbox interact.
        *   _Examples:_ VRP: `1017879`; VRP2.txt#11380.
*   **Origin/Context Confusion:** Sandboxed frames (especially `allow-same-origin`) potentially ending up in unexpected processes or sharing execution contexts improperly.
    *   **VRP Pattern (Context Reuse):** Sandboxed `allow-same-origin` iframe navigated from initial `about:blank` reusing the non-sandboxed `about:blank`'s execution context/type system, allowing the sandboxed frame to modify prototypes affecting the initial `about:blank` document. (VRP2.txt#6788).

## 3. Further Analysis and Potential Issues
*   **Flag Combinations:** Do specific combinations (e.g., `allow-scripts allow-popups` without `allow-top-navigation`) lead to unexpected interactions?
*   **Inheritance Logic:** Deep dive into the timing between CSP application (`DocumentLoader::InitializeWindow`) and `window.open` calls (`CreateNewWindow`). How do intermediate navigations within the sandboxed frame affect the flags read by `CreateNewWindow`?
*   **`allow-same-origin`:** Scrutinize process allocation, SiteInstance grouping, and type system isolation, especially concerning navigation from the initial `about:blank`.
*   **Parsing Robustness:** Are there edge cases in `HTMLIFrameElement::ParseSandboxAttribute`?
*   **Interaction with Navigation Logic:** Ensure *final* computed sandbox flags (`NavigationPolicyContainerBuilder::ComputeSandboxFlags`) are consistently enforced at *all* relevant check points (`FrameLoader::CanNavigate`, download checks, popup creation logic), particularly after redirects or CSP header processing.
*   **Platform Differences (Android Intents):** Thoroughly review the `IntentHelper` and related Android-specific navigation code paths for sandbox flag checks. See [intents.md](intents.md).
*   **Data URL Interaction:** How does sandbox inheritance work when a `data:` URL is the source, especially when opened via `window.open` or link clicks? (Related to VRP2.txt#7507).

## 4. Code Analysis
*   `HTMLIFrameElement::ParseSandboxAttribute` (Blink): Parses `sandbox` attribute string, sets initial flags on the element.
*   `DocumentLoader::InitializeWindow` (Blink): Called during commit. Creates/Applies `ContentSecurityPolicy` (`CreateCSP`, `SetContentSecurityPolicy`), which updates active sandbox flags in the `SecurityContext` via `ContentSecurityPolicy::ApplySandboxFlags`. **Crucial timing relative to script execution.**
*   `LocalDOMWindow::open` (Blink): Calls `FrameTree::FindOrCreateFrameForNavigation`. Checks sandbox flags (`CanShowModalDialog`, `CanOpenModalWindow`).
*   `FrameTree::FindOrCreateFrameForNavigation` (Blink): Finds existing frame or calls Blink's `CreateNewWindow`.
*   `CreateNewWindow` (`create_window.cc`, Blink): Reads opener's *current* sandbox flags (`IsSandboxed`, `GetSandboxFlags`) and `allow-popups-to-escape-sandbox` to determine *initial* popup flags. Calls `ChromeClient::CreateWindow`. **Relies on opener's current state, which might be stale or incorrect due to timing.**
*   `ChromeClientImpl::CreateWindowDelegate` (Blink): Bridge, passes initial flags to browser.
*   `RenderFrameHostImpl::CreateNewWindow` (Browser): Delegates window creation.
*   `RenderFrameHostImpl::InitializePolicyContainerHost` (Browser): Sets up popup's initial empty document policies, cloning opener's policies and combining initial flags from Blink with opener's CSP sandbox flags. **Applies potentially incorrect initial flags.**
*   `NavigationPolicyContainerBuilder::ComputeSandboxFlags` (Browser): Computes *final* flags before commit, combining iframe attributes, CSP headers, etc. **This happens later in the navigation process.**
*   `Frame::IsSandboxed(flag)` (Blink): Checks currently active flags in `SecurityContext`. Used by various permission checks (e.g., `CanDisplayModalDialog`, `CanOpenWindow`).
*   `FrameLoader::CanNavigate` (Blink): Checks `allow-top-navigation*`. **Needs to use the final, committed sandbox flags.**
*   `DownloadManagerImpl::InterceptNavigation` (Browser): Checks `allow-downloads`. **Needs final flags.**
*   `IntentHelper::MaybeHandleIntent` (Android): Handles `intent://` navigation. **Needs robust sandbox flag checking.**

## 5. Areas Requiring Further Investigation
*   **Verify intermediate navigation timing:** Confirm exact point during `DocumentLoader::CommitNavigation` where `InitializeWindow` (and CSP sandbox application) occurs relative to script execution that might call `window.open`.
*   **Browser-Side `RenderFrameHostDelegate::CreateNewWindow`:** Analyze `WebContentsImpl` implementation details for flag propagation.
*   **Android `intent://` handling:** Complete flow analysis, especially regarding `data:` URI nesting and popup interactions. How are flags passed/checked?
*   **`allow-top-navigation` enforcement:** vs. CSP header timing. Where exactly are navigation checks performed relative to `ComputeSandboxFlags`?
*   **`allow-downloads` enforcement:** vs. `noopener` interaction (VRP2.txt#1105). Does the `noopener` context affect download checks?
*   **`allow-same-origin` isolation:** Examine `SiteInstance` logic and `V8WindowShell::Initialize` regarding context reuse for `about:blank` vs. sandboxed same-origin navigations (VRP2.txt#6788).

## 6. Related VRP Reports
*   **`allow-popups-to-escape-sandbox` Bypass:** VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992, #212, #12191
*   **`allow-downloads` Bypass:** VRP: `40060695`; VRP2.txt#11682, #1105
*   **`allow-top-navigation` Bypass:** VRP: `40053936` (via redirect); VRP2.txt#4247, #8387 (via CSP header), VRP2.txt#8121 (via events), #4910 (via `allow-top-navigation` header - likely same root cause as #4247)
*   **Intent Scheme Bypass (Android):** VRP: `1365100`; VRP2.txt#7507, #10199, #15706, #15724, #8642
*   **`javascript:` URI Bypass:** VRP: `1017879`; VRP2.txt#11380
*   **Context Reuse/Type Confusion (`allow-same-origin`):** VRP2.txt#6788

## 7. Cross-References
*   [navigation.md](navigation.md)
*   [content_security_policy.md](content_security_policy.md)
*   [intents.md](intents.md)
*   [downloads.md](downloads.md)
*   [site_instance.md](site_instance.md)

*(Note: Sandbox bypasses often interact with other features like Navigation, CSP, or platform-specific mechanisms like Android Intents)*