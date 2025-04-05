# Component: Blink > PerformanceAPIs

## 1. Component Focus
*   Focuses on the implementation of various Performance APIs, such as Resource Timing, Navigation Timing, User Timing, Paint Timing, etc.
*   Involves collecting and exposing timing and resource metadata to web pages.
*   Relevant files might include:
    *   `third_party/blink/renderer/core/timing/performance.cc`
    *   `third_party/blink/renderer/core/timing/performance_resource_timing.cc`
    *   Code related to specific timing metrics and observers.

## 2. Potential Logic Flaws & VRP Relevance
*   **Information Leaks (XS-Leaks):** Exposing cross-origin information through timing data, resource sizes, or redirect details that should normally be restricted by SOP.
    *   Leaking redirect URLs or timing information (VRP #40054148).
    *   Leaking resource size information through inconsistent handling or interaction with other features (e.g., redirects, range requests - covered more in `resource_timing.md` potentially, but relevant here).
    *   Leaking visited status through timing differences in paint or navigation events (potentially overlapping with history leak vectors).
*   Incorrect filtering or redaction of cross-origin data before exposing it via Performance APIs.
*   Bypassing restrictions like `Timing-Allow-Origin`.

## 3. Further Analysis and Potential Issues
*   *(Detailed analysis of how different Performance APIs handle cross-origin resources, redirects, and caching.)*
*   Are there specific timing metrics that are more prone to side-channel leaks?
*   How do error conditions or unusual response statuses (e.g., 204 No Content) affect Performance API reporting and potential leaks?

## 4. Code Analysis
*   *(Specific code snippets related to creating PerformanceEntry objects, applying cross-origin checks (`passesTimingAllowOriginCheck`), and handling redirects in Resource Timing to be added.)*

## 5. Areas Requiring Further Investigation
*   Thorough review of all Performance APIs for potential cross-origin information leakage vectors, especially concerning redirects, caching, and error states.
*   Interaction between Performance APIs and newer features like Service Workers, Portals, Fenced Frames.
*   Potential timing side-channels related to processing or reporting performance data.

## 6. Related VRP Reports
*   VRP #40054148 (P1, $0): performance API reveals information about redirects (XS-Leak)
*   VRP #1343401: Security: ResourceTiming entries are not generated for responses with 204, 205 status codes when loaded in a iframe (Related information leak via lack of entry)

*(This list should be reviewed against VRP.txt/VRP2.txt for completeness regarding Performance API reports).*