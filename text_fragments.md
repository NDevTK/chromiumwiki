# Component: Text Fragments

## 1. Component Focus
*   **Functionality:** Implements the Text Fragments specification ([Spec](https://wicg.github.io/scroll-to-text-fragment/)), allowing URLs to contain a fragment directive (`#:~:text=...`) that instructs the browser to find specific text snippets on a page, scroll them into view, and highlight them.
*   **Key Logic:** Parsing the text fragment directive from the URL fragment, searching the document's text content for the specified snippets (`TextFragmentFinder`), handling scrolling and highlighting (`TextFragmentSelectorGenerator`), ensuring security and privacy constraints (e.g., only navigating cross-origin if the link is user-activated).
*   **Core Files:**
    *   `third_party/blink/renderer/core/fragment_directive/`: Core logic for parsing directives and initiating searches.
    *   `third_party/blink/renderer/core/text_fragments/`: Logic for finding text matches (`text_fragment_finder.cc`) and generating selectors (`text_fragment_selector_generator.cc`).
    *   `third_party/blink/renderer/core/page/scrolling/scroll_into_view_options.cc`: Involved in scrolling the matched fragment into view.
    *   `content/browser/renderer_host/navigation_request.cc`: Potentially involved in handling the fragment directive during navigation.

## 2. Potential Logic Flaws & VRP Relevance
*   **Information Leaks / Side Channels:** The browser's behavior when attempting to find, scroll to, or highlight a text fragment could potentially leak information about the content of a cross-origin page, even if the full navigation is blocked or restricted.
    *   **VRP Pattern (Scroll Inference):** Differences in scroll position or behavior after navigating to a URL with a text fragment directive could potentially leak information about whether the text exists on a cross-origin page (VRP: `1400345`). Requires careful analysis of scrolling logic and timing. See [privacy.md](privacy.md).
*   **Incorrect Parsing/Matching:** Errors in parsing the complex `#:~:text=` syntax or in matching the specified text snippets within the document could lead to incorrect highlighting, navigation failures, or potentially security issues if parsing errors create exploitable states.
*   **Performance Issues / DoS:** Extremely complex or numerous text fragment directives could cause performance issues or denial of service during text searching.
*   **Bypassing Security Restrictions:** Text fragment navigation potentially bypassing other security mechanisms like CSP frame-ancestors or CORP/COEP if not handled correctly within the navigation flow.

## 3. Further Analysis and Potential Issues
*   **Cross-Origin Behavior:** Deep dive into the conditions under which text fragment navigation and highlighting are permitted for cross-origin URLs. Is the user activation requirement robust? Can timing differences in the search/scroll process leak information even if the final scroll/highlight doesn't occur cross-origin? (VRP: `1400345`).
*   **Text Searching Logic (`TextFragmentFinder`):** Analyze the text matching algorithm for efficiency and potential edge cases with complex text, languages, or DOM structures.
*   **Scrolling Logic:** Review how the scroll-into-view behavior is implemented for text fragments. Can the scroll position be measured accurately enough cross-origin to create a side channel? (VRP: `1400345`).
*   **Interaction with Navigation:** How does the fragment directive parser interact with the main URL parsing and navigation logic (`NavigationRequest`)? Are security policies checked correctly before attempting the text fragment search/scroll?

## 4. Code Analysis
*   `FragmentDirectiveParser`: Parses the `#:~:` fragment part of the URL.
*   `TextFragmentFinder`: Implements the logic to find specified text snippets within the document.
*   `TextFragmentSelectorGenerator`: Involved in highlighting the found text.
*   `ScrollableArea::ScrollToTextFragment`: Handles scrolling the fragment into view. Check timing and intermediate states.
*   `NavigationRequest`: How fragment directives influence navigation decisions.

## 5. Areas Requiring Further Investigation
*   **Scroll Timing Side Channel:** Investigate whether precise timing measurements of scroll events or related rendering updates after a text fragment navigation attempt can leak cross-origin information (VRP: `1400345`).
*   **Parsing Edge Cases:** Fuzz the text fragment directive parser with complex and malformed syntax.
*   **Interaction with other features:** Test interactions with iframes, sandboxing, CSP, and other features that might affect navigation or content visibility.

## 6. Related VRP Reports
*   VRP: `1400345` (Scroll behavior inference leaking cross-origin information)

*(See also [privacy.md](privacy.md), [navigation.md](navigation.md))*