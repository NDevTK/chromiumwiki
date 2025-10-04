# Fenced Frames Security Model

Fenced Frames are an HTML feature designed for embedding third-party content, particularly from advertising auctions (like Protected Audience API) or Shared Storage, without allowing communication between the embedding page and the embedded content. This provides a strong privacy boundary, preventing the embedded content from learning about the user's context on the embedding page and vice-versa.

This analysis is based on the renderer-side implementation in `third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.cc` and the browser-side implementation in `content/browser/fenced_frame/`.

## Core Architectural Principle: Nested Frame Trees

The fundamental security and isolation of Fenced Frames comes from its core architecture. Unlike a traditional `<iframe>` which creates a new frame *within* the same frame tree as its parent, a `<fencedframe>` element instantiates an entirely new, separate **`FrameTree`** in the browser process.

-   **`FencedFrame` Object**: A browser-side C++ `FencedFrame` object is created for each `<fencedframe>` element. This object owns and manages the new, nested `FrameTree`.
-   **Complete Separation**: This nested tree has its own `NavigationController` and session history. This architectural separation is the foundation for all other security properties, as it ensures there is no direct path between the two browsing contexts.

## Communication Lockdown

Fenced Frames are designed to be a "black box" to the embedding page. This is enforced in several ways:

-   **No DOM Access**: The `HTMLFencedFrameElement` in the renderer **does not expose `contentWindow` or `contentDocument` properties**. This is the most critical lockdown, as it severs the primary vector for script-based communication and DOM manipulation that exists with iframes.
-   **No `postMessage`**: There is no mechanism to `postMessage` into or out of a fenced frame.
-   **Limited Size Influence**: While the embedder can suggest a size, the final size of the frame can be coerced by the browser (e.g., for opaque-ads mode) to a set of allowed dimensions, preventing the embedder from using size to leak information.

## Navigation Control

Navigation within a fenced frame is strictly mediated by the browser process to prevent the embedder from using navigation as a communication channel.

1.  **Navigation via `config`**: The embedder does not set a `src` attribute. Instead, it provides a `FencedFrameConfig` object. This object contains either a URN (`urn:uuid:...`) or a URL from a pre-approved list.
2.  **Browser-Side Validation**: The renderer sends a Mojo IPC to the browser-side `FencedFrame::Navigate` method. This method performs strict validation:
    - It ensures the URL is a valid `urn:uuid:` or another specifically permitted format. Any attempt to navigate to an arbitrary URL is rejected, and the renderer process is terminated for sending a bad message.
3.  **Opaque Initiator and New Browsing Instance**: When the browser initiates the navigation for the fenced frame, it does so with:
    - A new, **opaque initiator origin**. This prevents the `Referer` header from leaking the embedding page's origin to the destination server.
    - The `force_new_browsing_instance` flag set to `true`. This ensures the content loads in a completely separate security context, severing any remaining script connections (like `window.opener`).

## Privacy-Preserving Reporting

Since direct communication is blocked, a dedicated and limited reporting mechanism is provided for use cases like ad attribution.

-   **`FencedFrameReporter`**: All reporting is funneled through the browser-side `FencedFrameReporter` object.
-   **Pre-Registered Destinations**: The fenced frame content cannot report to an arbitrary URL. It can only send data to destinations (e.g., "buyer", "seller") that were pre-registered by an ad auction worklet using `registerAdBeacon()`.
-   **Sanitized Beacons**: When `SendReport` is called, the reporter constructs a new network request with sanitized data:
    - **No Credentials**: `credentials_mode` is set to `kOmit`, so no cookies are sent.
    - **Scrubbed Initiator/Referrer**: The initiator and `Referer` are set to the origin of the worklet that registered the beacon, not the embedding page.
-   **Integration with Privacy Sandbox APIs**: The reporter is a gateway to other privacy-preserving APIs like the Attribution Reporting API and the Private Aggregation API, allowing measurement without direct data leakage.

In summary, the Fenced Frame security model is a significant step up from iframes. By creating a nested browsing context and having the browser process mediate all navigation and communication, it provides a robust boundary that protects user privacy by preventing the embedder and the embedded content from learning about each other.