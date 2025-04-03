# Fenced Frames

This page details the security aspects of Fenced Frames in Chromium.

## Core Concepts

Fenced Frames are a web platform feature that provides enhanced privacy and security for embedded content. They are designed to prevent the embedding page from accessing the content of the frame and vice versa.

## Potential Security Issues

-   Compromised renderer can bypass feature flag checks and create FencedFrames.
-   `FencedFrame::Navigate` can navigate to `file://` and `chrome://` URLs, potentially leading to arbitrary file read.

### Security Considerations

-   A vulnerability existed where FencedFrames were reachable from a compromised renderer due to lacking `features::isEnabled(kFencedFrames)` checks in the Browser Process. Also, `FencedFrame::Navigate` could navigate to `file://` and `chrome://` origins. This issue has been fixed. (VRP2.txt)
    -   Fixed in commit: 40057525

### Related Files

-   `third_party/blink/common/features.cc`
-   `content/browser/renderer_host/render_frame_host_impl.cc`

### Further Investigation

-   Ensure that all feature flags related to Fenced Frames are properly checked in both the renderer and browser processes.
-   Investigate the potential for other vulnerabilities related to the `FencedFrame::Navigate` method.
-   Analyze the interaction between Fenced Frames and other security mechanisms, such as CSP and COOP.