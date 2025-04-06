# Component: Fenced Frames

## 1. Component Focus
*   **Functionality:** Implements the Fenced Frames API ([Explainer](https://github.com/WICG/fenced-frame)), a mechanism for embedding content (typically ads) without allowing communication between the embedder and the embedded frame, aiming to prevent cross-site tracking and information leaks. Uses URNs (`urn:uuid:...`) for navigation.
*   **Key Logic:** Handling the `<fencedframe>` element, managing navigation to URNs, enforcing communication boundaries, interaction with features like Shared Storage API, reporting mechanisms (Fenced Frames reporting API). Ensuring isolation from the embedder context.
*   **Core Files:**
    *   `content/browser/fenced_frame/`: Core browser-side implementation (e.g., `fenced_frame.cc`, `fenced_frame_url_mapping.cc`).
    *   `third_party/blink/renderer/core/html/html_fenced_frame_element.cc`: Renderer-side element implementation.
    *   `third_party/blink/renderer/core/frame/fenced_frame/`: Renderer-side frame logic.
    *   `content/browser/renderer_host/navigation_request.cc`: Handles navigation, including URN resolution.

## 2. Potential Logic Flaws & VRP Relevance
*   **Isolation Bypass / Communication Leaks:** Flaws allowing unintended communication or data leakage between the fenced frame and the embedder page, defeating the core purpose.
    *   **VRP Pattern Concerns:** Could timing attacks, side channels (e.g., via shared browser resources, cache timing), or complex interactions with other APIs (e.g., Reporting API) leak information across the boundary?
*   **Navigation Policy Bypasses:** Fenced frames potentially bypassing navigation restrictions applied to normal iframes.
    *   **VRP Pattern (Loading Restricted Schemes):** Fenced frames were found to bypass checks and navigate to disallowed `file://` URLs (VRP2.txt#225). Fenced frames loading local directories (VRP: `1454937`, VRP2.txt#12642). Requires strict URL validation in navigation logic (`NavigationRequest::CheckUrlBasedPermissions`).
*   **Interaction with Other Features:** Unexpected interactions with features like Picture-in-Picture, DevTools, or extensions.
    *   **VRP Pattern (PiP Interaction):** Interaction between Fenced Frames and Document Picture-in-Picture leading to origin confusion/spoofing (VRP: `40062954`, VRP2.txt#7262). See [picture_in_picture.md](picture_in_picture.md).
*   **URN Mapping/Resolution Issues:** Vulnerabilities in the mapping or resolution of `urn:uuid:` URLs to actual content URLs.

## 3. Further Analysis and Potential Issues
*   **Communication Boundary Enforcement:** Deep dive into mechanisms preventing communication (postMessage, DOM access, etc.) between the fenced frame and embedder. Are there subtle ways to bypass this (e.g., timing attacks, shared resources)?
*   **Navigation Logic (`NavigationRequest`):** How are navigations within fenced frames handled differently from normal iframes? Are all necessary security checks (scheme checks, policy checks) applied correctly, especially considering the URN mapping layer? (VRP2.txt#225, VRP: `1454937`).
*   **URN Mapping Security (`FencedFrameURLMapping`):** How is the mapping between URNs and content URLs managed? Is this mapping protected from tampering by the embedder or other contexts?
*   **Interaction with Shared Storage API:** Analyze the security implications of allowing fenced frames to access Shared Storage.
*   **Reporting API Security:** Ensure the Fenced Frames reporting mechanisms don't leak sensitive cross-context information.

## 4. Code Analysis
*   `FencedFrame`: Browser-side representation of a fenced frame.
*   `FencedFrameURLMapping`: Maps URNs to actual URLs.
*   `NavigationRequest`: Handles navigation, including `CheckUrlBasedPermissions`. Needs robust checking for fenced frame navigations (VRP2.txt#225, VRP: `1454937`).
*   `HTMLFencedFrameElement`: Renderer-side element logic.
*   Code related to Shared Storage API interaction.
*   Code related to Fenced Frame Reporting API.

## 5. Areas Requiring Further Investigation
*   **Communication Side Channels:** Actively search for potential side channels (timing, cache, shared resources) that could leak data across the fenced frame boundary.
*   **Navigation Security Checks:** Verify that *all* relevant security checks (scheme restrictions, policy enforcement) applied during normal navigation are also correctly applied to the resolved URL during fenced frame navigation.
*   **URN Mapping Integrity:** Ensure the URN mapping mechanism is robust against manipulation.
*   **Interaction Testing:** Test interactions with a wide range of browser features (DevTools, extensions, PiP, other APIs).

## 6. Related VRP Reports
*   VRP: `40062954` / VRP2.txt#7262 (Interaction with PiP leading to origin confusion)
*   VRP: `1454937` / VRP2.txt#12642 (Loading local directories via fenced frame)
*   VRP2.txt#225 (Navigation to file:// URLs)

*(See also [site_isolation.md](site_isolation.md), [navigation.md](navigation.md), [picture_in_picture.md](picture_in_picture.md))*