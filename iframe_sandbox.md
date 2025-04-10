# Component: Blink > SecurityFeature > IFrameSandbox

## 1. Component Focus
*   **Functionality:** Implements the `sandbox` attribute for `<iframe>` elements ([Spec](https://html.spec.whatwg.org/multipage/iframe-embed-object.html#attr-iframe-sandbox)), restricting the capabilities of the embedded content. Also interacts with the `sandbox` directive in Content Security Policy (CSP).
*   **Key Logic:** Parsing the `sandbox` attribute (`HTMLIFrameElement::ParseSandboxAttribute`), combining attribute flags with CSP sandbox flags (`NavigationPolicyContainerBuilder::ComputeSandboxFlags`), inheriting flags to nested frames/popups (partly in `CreateNewWindow` and `FrameTree::FindOrCreateFrameForNavigation`?), enforcing restrictions during navigation (`NavigationRequest`, `FrameLoader`), popup creation (`LocalDOMWindow::open`), script execution, form submission, downloads, etc.
*   **Core Files:**
    *   `third_party/blink/renderer/core/frame/local_dom_window.cc` (Implements `window.open`)
    *   `third_party/blink/renderer/core/page/create_window.cc` (Helper for `window.open`, contains `CreateNewWindow`)
    *   `third_party/blink/renderer/core/page/frame_tree.cc` (Handles `FindOrCreateFrameForNavigation`)
    *   `third_party/blink/renderer/core/html/html_iframe_element.cc` (Parsing attribute)
    *   `third_party/blink/renderer/core/frame/sandbox_flags.h`
    *   `content/browser/renderer_host/navigation_policy_container_builder.cc` (Computes final flags before commit)
    *   `content/browser/renderer_host/navigation_request.cc` (Uses final flags)
    *   `third_party/blink/renderer/core/loader/frame_loader.cc`

## 2. Potential Logic Flaws & VRP Relevance
*   **Bypassing Specific Sandbox Flags:** Finding ways to perform actions restricted by flags like `allow-scripts`, `allow-popups`, `allow-top-navigation`, `allow-downloads`, `allow-same-origin`. This often stems from incorrect flag calculation/inheritance *before* the final commit, or specific interactions.
    *   **VRP Pattern (`allow-popups-to-escape-sandbox` Bypass):** Popups opened from sandboxed frames incorrectly inheriting opener's capabilities or losing restrictions. **Analysis of `CreateNewWindow` shows it correctly determines the initial sandbox flags to pass to the browser based on the opener's flags and the presence/absence of `allow-popups-to-escape-sandbox`.** Bypass likely involves **intermediate navigations** within the sandboxed context before `window.open` is called (changing the context `CreateNewWindow` uses), or browser-side logic incorrectly applying the initial flags during popup creation/navigation. (VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992).
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
*   **Inheritance Logic:** Focus on **intermediate navigation scenarios** before popup creation. How does the context used by `CreateNewWindow` change if the frame calling `window.open` navigated after the initial sandboxed load? Also investigate **browser-side popup creation logic** (`ChromeClient::CreateWindow` implementation and how it uses the initial flags).
*   **`allow-same-origin`:** Request handling, process allocation.
*   **Parsing Robustness.**
*   **Interaction with Navigation Logic:** Enforcement of *final* computed flags at all stages.
*   **Platform Differences (Android Intents).**

## 4. Code Analysis
*   `LocalDOMWindow::open`: Handles `noopener` based on features string and context (Fenced Frames, Blob URLs). Calls `FrameTree::FindOrCreateFrameForNavigation`. **Does not directly handle sandbox flag inheritance for popups.**
*   `FrameTree::FindOrCreateFrameForNavigation`: Finds existing frame or calls `CreateNewWindow`. **Likely involved in setting initial flags for new popup windows based on opener and features.**
*   `CreateNewWindow` (`create_window.cc`): Performs initial checks (popup blocker, sandbox `allow-popups`), **correctly determines initial `sandbox_flags` to pass to browser based on opener's flags and `allow-popups-to-escape-sandbox`**, calls `ChromeClient::CreateWindow`.
*   `NavigationPolicyContainerBuilder::ComputeSandboxFlags`: **Computes the final effective sandbox flags before navigation commit, combining iframe attributes, CSP, and potentially inherited flags.**
*   `HTMLIFrameElement::ParseSandboxAttribute`: Parses attribute string.
*   `Frame::IsSandboxed(flag)`: Checks currently active flags.
*   `FrameLoader::CanNavigate`: Checks `allow-top-navigation*`.
*   Download / Android Intent handling code.

## 5. Areas Requiring Further Investigation
*   **Thorough review of popup sandbox inheritance in `FrameTree::FindOrCreateFrameForNavigation` and related navigation code.** How are flags determined when `allow-popups-to-escape-sandbox` is set/not set? How do intermediate navigations (e.g., to `about:blank`) affect this? (VRP: `40069622`, `40057525`).
*   **Browser-Side Popup Creation:** Review the implementation of `ChromeClient::CreateWindow` and subsequent browser-side navigation logic to see how the initial sandbox flags are applied and potentially modified.
*   **Android `intent://` handling:** Analyze the complete flow of handling `intent://` URLs initiated from sandboxed frames.
*   **`allow-top-navigation` enforcement:** Re-evaluate checks in `FrameLoader::CanNavigate` against CSP header scenarios (VRP2.txt#4247).
*   **`allow-downloads` enforcement:** Check download paths involving `noopener` (VRP2.txt#1105).
*   **`allow-same-origin` isolation:** Audit context/process management (VRP2.txt#6788).

## 6. Related VRP Reports
*   **`allow-popups-to-escape-sandbox` Bypass:** VRP: `40069622`, `40057525`; VRP2.txt#7849, #11992
*   **`allow-downloads` Bypass:** VRP: `40060695`; VRP2.txt#11682, #1105
*   **`allow-top-navigation` Bypass:** VRP: `40053936` (via redirect); VRP2.txt#4247, #8387 (via CSP header), VRP2.txt#8121 (via events)
*   **Intent Scheme Bypass (Android):** VRP: `1365100`; VRP2.txt#7507, #10199, #15706
*   **`javascript:` URI Bypass:** VRP: `1017879`; VRP2.txt#11380
*   **Context Reuse/Type Confusion (`allow-same-origin`):** VRP2.txt#6788

*(Note: Sandbox bypasses often interact with other features like Navigation, CSP, or platform-specific mechanisms like Android Intents)*