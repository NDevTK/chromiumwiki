# Component: Fenced Frames

## 1. Component Focus
*   **Functionality:** Implements the Fenced Frames API ([Explainer](https://github.com/WICG/fenced-frame)), a mechanism for embedding content (typically ads) without allowing communication between the embedder and the embedded frame, aiming to prevent cross-site tracking and information leaks. Uses URNs (`urn:uuid:...`) for navigation.
*   **Key Logic:** Handling the `<fencedframe>` element, managing navigation to URNs, enforcing communication boundaries, interaction with features like Shared Storage API, reporting mechanisms (Fenced Frames reporting API). Ensuring isolation from the embedder context.
*   **Core Files:**
    *   `content/browser/fenced_frame/`: Core browser-side implementation (e.g., `fenced_frame.cc`, `fenced_frame_url_mapping.cc`).
    *   `third_party/blink/renderer/core/html/html_fenced_frame_element.cc`: Renderer-side element implementation.
    *   `third_party/blink/renderer/core/frame/fenced_frame/`: Renderer-side frame logic.
    *   `content/browser/renderer_host/navigation_request.cc`: Handles navigation, including URN resolution and permission checks.
    *   `content/browser/devtools/protocol/fenced_frame_handler.cc`: DevTools protocol handler for Fenced Frames.

## 2. Potential Logic Flaws & VRP Relevance
*   **Isolation Bypass / Communication Leaks:** Flaws allowing unintended communication or data leakage between the fenced frame and the embedder page, defeating the core purpose.
    *   **VRP Pattern Concerns:** Could timing attacks, side channels (e.g., via shared browser resources, cache timing), or complex interactions with other APIs (e.g., Reporting API, Shared Storage) leak information across the boundary? Check access control for DOM properties (`window.top`, `parent`, etc.) and `postMessage` routing.
*   **Navigation Policy Bypasses:** Fenced frames potentially bypassing navigation restrictions applied to normal iframes, especially during the asynchronous URN resolution process.
    *   **VRP Pattern (Loading Restricted Schemes):** Fenced frames were found to bypass checks and navigate to disallowed `file://` URLs (VRP2.txt#225) or load local directories (VRP: `1454937`, VRP2.txt#12642). This highlights the need for strict URL validation in navigation logic (`NavigationRequest::CheckUrlBasedPermissions`, `SchemeNavigationThrottle`, etc.) applied to the *resolved* URL obtained *after* URN mapping completes.
*   **Interaction with Other Features:** Unexpected interactions with features like Picture-in-Picture, DevTools, or extensions.
    *   **VRP Pattern (PiP Interaction):** Interaction between Fenced Frames and Document Picture-in-Picture leading to origin confusion/spoofing (VRP: `40062954`, VRP2.txt#7262). See [picture_in_picture.md](picture_in_picture.md). How does PiP logic handle URNs or interact with the `FencedFrameURLMapping`?
*   **URN Mapping/Resolution Issues:** Vulnerabilities in the mapping or resolution of `urn:uuid:` URLs to actual content URLs stored in `FencedFrameURLMapping`. Is the mapping instance correctly isolated and protected?

## 3. Further Analysis and Potential Issues
*   **Communication Boundary Enforcement:** Deep dive into mechanisms preventing communication (postMessage, DOM access, etc.) between the fenced frame and embedder within Blink (`third_party/blink/renderer/core/frame/fenced_frame/`, `html_fenced_frame_element.cc`). Are there subtle ways to bypass this (e.g., timing attacks, shared resources, inherited properties)?
*   **Navigation Logic (`NavigationRequest`):** How are navigations within fenced frames handled differently from normal iframes? Key areas include:
    *   The asynchronous URN resolution (`FencedFrameURLMapping::ResolveURN`, signaled by `NavigationRequest::is_deferred_on_fenced_frame_url_mapping_`).
    *   Ensuring all necessary security checks (`NavigationRequest::CheckUrlBasedPermissions`, `SchemeNavigationThrottle`, etc.) are applied *after* the mapping completes in `NavigationRequest::OnFencedFrameURLMappingComplete`, using the resolved properties. (Related VRP2.txt#225, VRP: `1454937`).
    *   Potential race conditions if navigation is cancelled/redirected while mapping is deferred.
*   **URN Mapping Security (`FencedFrameURLMapping`):** How is the mapping between URNs and content URLs managed (`MapUrnToUrl`, `AddObserver`, `RemoveObserver`)? Where are `FencedFrameURLMapping` instances stored? `NavigationRequest::GetFencedFrameURLMap` mentions accessing the *outer* frame tree's mapping - is this access appropriately controlled? Is the mapping protected from tampering or unauthorized access by the embedder or other renderer contexts?
*   **Interaction with Shared Storage API:** Analyze the security implications of allowing fenced frames to access Shared Storage. Examine code paths involving `shared_storage::SharedStorageManager` within `content/browser/fenced_frame/`.
*   **Reporting API Security:** Ensure the Fenced Frames reporting mechanisms (`FencedFrameReporter`) don't leak sensitive cross-context information.
*   **DevTools Interaction:** How does DevTools (`FencedFrameHandler`) interact with fenced frames? Can DevTools bypass isolation or expose sensitive information?

## 4. Code Analysis
*   `content/browser/fenced_frame/fenced_frame.cc`: Browser-side representation and core logic for a fenced frame.
*   `content/browser/fenced_frame/fenced_frame_url_mapping.cc`: Manages the mapping of URNs to actual URLs (`MapUrnToUrl`, `ResolveURN`). Investigate its lifecycle management and potential for cross-context access.
*   `content/browser/renderer_host/navigation_request.cc`: Handles navigations.
    *   `NeedFencedFrameURLMapping()`: Checks if the URL is a `urn:uuid:`.
    *   `GetFencedFrameURLMap()`: Retrieves mapping, noting it accesses the *outer* frame tree's map.
    *   `is_deferred_on_fenced_frame_url_mapping_`: Flag indicating asynchronous mapping is in progress.
    *   `OnFencedFrameURLMappingComplete()`: Callback executed *after* mapping finishes. **Security checks (`CheckUrlBasedPermissions`, throttles) must correctly use the properties resolved here.**
    *   Focus on the sequence: `NeedFencedFrameURLMapping` -> deferral -> `OnFencedFrameURLMappingComplete` -> `BeginNavigationImpl` (or similar continuation) -> security checks.
*   `third_party/blink/renderer/core/html/html_fenced_frame_element.cc`: Renderer-side element logic. Check for potential bypasses of communication restrictions.
*   `third_party/blink/renderer/core/frame/fenced_frame/`: Renderer-side frame logic. Examine isolation mechanisms, e.g., checks preventing access to `window.top`, `window.parent`, `window.frames` within `blink::LocalDOMWindow` or related frame proxy logic. Analyze `postMessage` routing.
*   `content/browser/fenced_frame/fenced_frame_reporter.cc`: Handles reporting API integration. Check for potential info leaks.
*   Code involving `shared_storage::SharedStorageManager` within `content/browser/fenced_frame/`.

## 5. Areas Requiring Further Investigation
*   **Communication Side Channels:** Actively search for potential side channels (timing, cache, shared resources like `localStorage` if accessible, performance APIs) that could leak data across the fenced frame boundary.
*   **Navigation Security Checks Robustness:** Verify that security checks within `NavigationRequest` (e.g., `CheckUrlBasedPermissions`, `SchemeNavigationThrottle`) are consistently and correctly applied using the *resolved* properties *after* `OnFencedFrameURLMappingComplete` executes.
*   **Navigation Timing/Race Conditions:** Investigate the handling of navigation cancellation, redirection, or other state changes occurring *while* URN mapping is deferred (`is_deferred_on_fenced_frame_url_mapping_` is true). Are security states correctly maintained or re-evaluated?
*   **URN Mapping Integrity & Access Control:** Ensure the URN mapping mechanism (`FencedFrameURLMapping`) is robust against manipulation. Investigate the lifecycle and access control of its instances, especially considering access involves the *outer* frame tree. Can a renderer influence or read mappings it shouldn't?
*   **Interaction Testing:** Test interactions with a wide range of browser features (DevTools, extensions, PiP, other APIs like WebAuthn, Payments). How does PiP get the URL when initiated from a fenced frame (related VRP: `40062954`)?
*   **Audit Communication Boundary Enforcement:** Audit communication boundary enforcement in Blink, particularly access control checks for properties like `window.parent`/`top`/`frames` within `third_party/blink/renderer/core/frame/fenced_frame/` and related DOM window logic (`blink::LocalDOMWindow`, `WindowAgent`). Check message routing for `postMessage`.

## 6. Related VRP Reports
*   VRP: `40062954` / VRP2.txt#7262 (Interaction with PiP leading to origin confusion)
*   VRP: `1454937` / VRP2.txt#12642 (Loading local directories via fenced frame)
*   VRP2.txt#225 (Navigation to file:// URLs)

## 7. Cross-References
*   [site_isolation.md](site_isolation.md)
*   [navigation.md](navigation.md)
*   [picture_in_picture.md](picture_in_picture.md)
*   [shared_storage.md](shared_storage.md)