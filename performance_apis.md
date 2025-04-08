# Component: Performance APIs

## 1. Component Focus
*   **Functionality:** Implements various Performance APIs allowing web pages to measure performance metrics, including Navigation Timing, Resource Timing, User Timing (`performance.mark`, `performance.measure`), Server Timing, `PerformanceObserver`, and potentially lower-level APIs related to event timing or long tasks.
*   **Key Logic:** Recording timestamps at various stages of navigation and resource loading (`PerformanceNavigationTiming`, `PerformanceResourceTiming`), allowing registration of observers (`PerformanceObserver`) to be notified of new entries, providing interfaces for user-defined marks and measures.
*   **Core Files:**
    *   `third_party/blink/renderer/core/timing/`: Core implementation of various performance interfaces (e.g., `performance.cc`, `performance_navigation_timing.cc`, `performance_resource_timing.cc`, `performance_observer.cc`).
    *   `third_party/blink/renderer/core/loader/`: Resource loading logic provides timing data.
    *   `content/browser/renderer_host/`: Browser process involvement in gathering some timing information (e.g., navigation start).

## 2. Potential Logic Flaws & VRP Relevance
*   **Cross-Origin Information Leaks (Side Channels):** The primary risk is leaking information about cross-origin resources or navigations that should be opaque according to SOP or CORP/COEP policies. This often happens via timing differences, presence/absence of entries, or specific attribute values.
    *   **VRP Pattern (Redirect Timing/URL Leaks):** Leaking details about cross-origin redirects via `PerformanceResourceTiming` entries or `PerformanceNavigationTiming`. `nextHopProtocol` leaking cross-origin redirect info (VRP2.txt#13061). Redirect timing leaks (VRP: `40054148`, VRP2.txt#14397). Leaking redirect URLs via WebGL texture errors (VRP2.txt#16520). Service Worker + Range requests leaking redirect info (VRP2.txt#14397).
    *   **VRP Pattern (History/Visited Link Leaks):** Using timing differences observable via Performance APIs (combined with other techniques like CSS transitions or Paint API) to infer visited link status (VRP: `1211002`, `680214`; VRP2.txt#12845).
    *   **VRP Pattern (Cache Timing):** Potentially using timing of resource loads (observable via `PerformanceResourceTiming`) to infer cross-origin caching status.
    *   **VRP Pattern (Resource Size Leaks):** Interaction with Service Workers and Range requests allowing inference of cross-origin resource size (VRP2.txt#14397).
    *   **VRP Pattern (Cross-Origin Object Leaks):** `fetch` combined with performance APIs potentially leaking properties of cross-origin objects (VRP2.txt#6873 related).
*   **Incorrect Timestamp Calculation/Reporting:** Errors in recording or calculating timestamps leading to inaccurate metrics (less likely a direct security bug, but could contribute to side channels).
*   **Resource Exhaustion (DoS):** Creating excessive observers or user timing entries overwhelming memory or processing.

## 3. Further Analysis and Potential Issues
*   **Cross-Origin Resource Timing:** Deep dive into the checks determining which timing properties are exposed for cross-origin resources (`PerformanceResourceTiming::AllowsTimingDetails`). Is the `Timing-Allow-Origin` header correctly enforced? Are there leaks even when details are restricted (e.g., through entry existence or filtered timing)?
*   **Navigation Timing:** Analyze how `PerformanceNavigationTiming` handles cross-origin redirects. Are sensitive timings (e.g., DNS lookup, connection time for the cross-origin redirect target) correctly hidden? (VRP2.txt#13061, #14397).
*   **PerformanceObserver Logic:** How are observer notifications delivered? Are there race conditions or potential leaks in the observer callback mechanism?
*   **Interaction with Service Workers:** How does `PerformanceResourceTiming` interact with resources fetched/served by a Service Worker? Are timings accurate and secure? (VRP2.txt#14397, #14773).
*   **Interaction with Caching:** Can timing APIs reveal information about the state of various caches (HTTP cache, code cache, etc.) cross-origin?

## 4. Code Analysis
*   `Performance`: Core `performance` object interface in Blink.
*   `PerformanceNavigationTiming`, `PerformanceResourceTiming`: Store and expose timing data for navigation and resources. Check access control logic, especially for cross-origin resources (`AllowsTimingDetails`).
*   `PerformanceObserver`: Handles observing performance entries.
*   `ResourceTimingInfo`: Data structure passed from network stack containing timing info.
*   `DocumentTiming`: Records key document lifecycle timestamps.

## 5. Areas Requiring Further Investigation
*   **Cross-Origin Redirect Leaks:** Systematically test `PerformanceResourceTiming` and `PerformanceNavigationTiming` with various cross-origin redirect chains (HTTP-HTTPS, HTTPS-HTTP, different origins) to look for leaks via exposed timings or `nextHopProtocol`.
*   **Timing Side Channels:** Explore potential timing leaks related to caching, service worker interaction, or specific API calls by measuring performance entry timings.
*   **`Timing-Allow-Origin` Enforcement:** Verify the implementation correctly parses and enforces the TAO header for cross-origin resource timing exposure.

## 6. Related VRP Reports
*   **Redirect Leaks:** VRP: `40054148`; VRP2.txt#14397 (`nextHopProtocol` leak via Perf API + SW Range), #13061 (`nextHopProtocol` leak).
*   **Visited Link/History Leaks:** VRP: `1211002`, `680214`; VRP2.txt#12845 (Often combined with CSS).
*   **Resource Size Leaks (Interaction):** VRP2.txt#14397, #14773 (Often combined with SW/Cache/Range).
*   **Cross-origin Object Leaks (Interaction):** VRP2.txt#6873 (Related to Fetch API).

*(Note: Performance API vulnerabilities are often subtle side channels requiring careful timing measurements and interaction with other browser features.)*