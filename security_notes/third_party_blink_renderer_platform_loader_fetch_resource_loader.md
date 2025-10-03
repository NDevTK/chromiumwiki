# Security Notes for `third_party/blink/renderer/platform/loader/fetch/resource_loader.cc`

The `ResourceLoader` class is the Blink-side endpoint for a single network request. For every resource fetch initiated by the `ResourceFetcher`, a `ResourceLoader` is created to manage its lifecycle. It acts as the client for the `mojom::URLLoader` interface, receiving events from the network service, processing them, and applying critical renderer-side security checks before passing the data to the `Resource` object.

## Core Security Responsibilities

1.  **Mojo URLLoaderClient Implementation**: `ResourceLoader` is the concrete implementation of the `URLLoaderClient` interface in the renderer. It is responsible for safely handling all asynchronous callbacks from the network service, such as `DidReceiveResponse`, `WillFollowRedirect`, and `DidFail`. The security of the renderer depends on this class correctly interpreting and acting on the information received from the (more privileged) network process.

2.  **Redirect Policy Enforcement**: This is one of the most critical security functions of the `ResourceLoader`. The `WillFollowRedirect` method is the gatekeeper for all redirects. It is responsible for:
    *   **Checking Security Policies**: Before following a redirect, it consults the `FetchContext` to perform CSP (`CheckCSPForRequest`) and other security checks (`CanRequest`) on the *new* URL. This is a fundamental check that prevents a request from being redirected to a location forbidden by the document's security policy.
    *   **Stripping Sensitive Headers**: It implements the logic to determine which headers (e.g., `Authorization`) must be stripped during a cross-origin redirect to prevent credential leakage. It populates the `removed_headers` vector that is sent back to the `URLLoader` in the network service.
    *   **Referrer Policy Application**: It calculates the new `Referer` header based on the document's referrer policy and the nature of the redirect.

3.  **Response Validation**: In `DidReceiveResponse`, the loader performs initial validation on the response headers.
    *   **"nosniff" Check**: It calls `CheckResponseNosniff` to enforce the `X-Content-Type-Options: nosniff` header. This is a critical security measure that prevents MIME-type confusion attacks, where a browser might misinterpret a resource (e.g., execute an HTML file that was declared as an image).

4.  **Data Handling and Body Loading**: It receives the response body via a data pipe and hands it off to a `ResponseBodyLoader`. This handoff must be done safely, ensuring that data is only processed after all security checks on the response headers have passed.

5.  **CNAME Alias Checking**: The `ShouldBlockRequestBasedOnSubresourceFilterDnsAliasCheck` method provides a defense against CNAME cloaking, a technique used by trackers to bypass ad blockers. When the network service reports that a hostname resolved through a chain of CNAMEs, this method checks each alias in the chain against the subresource filter. If any alias is blocked, the entire request is cancelled.

## Security-Critical Logic

*   **`WillFollowRedirect`**: The logic within this method is paramount. A bug here could lead to a CSP bypass, information disclosure (e.g., leaking an `Authorization` header), or a failure to correctly apply referrer policy. The interaction with `Context()->CanRequest()` is the primary security gate for the redirect destination.

*   **`DidReceiveResponseInternal`**: This method is the first point where the renderer process sees the response headers. It is responsible for initiating the `nosniff` check and other validations before any data is passed to the `ResponseBodyLoader`.

*   **State Management (`IsLoading`, `finished_`)**: The loader maintains its own state. A state confusion bug, where the loader believes a request is finished when it is not (or vice-versa), could lead to use-after-free vulnerabilities or cause security checks to be skipped.

*   **Error Handling (`HandleError`)**: This method is the central point for terminating a request due to a security violation (e.g., CSP block, `nosniff` block, CNAME block). It is responsible for ensuring that the load is properly cancelled and the `Resource` object is placed into an error state, preventing any partial or untrusted data from being used.

## Potential Attack Surface and Research Areas

*   **Redirect Logic Bypasses**: The complex logic in `WillFollowRedirect` is a key area for investigation. An attacker might try to craft a redirect chain that confuses the logic for stripping headers or for checking CSP, potentially leading to a security bypass. For example, a redirect from `a.com` -> `b.com` -> `a.com` might be handled differently than a direct request to `a.com`.
*   **Time-of-Check to Time-of-Use (TOCTOU) Races**: The `ResourceLoader`'s lifecycle is asynchronous. Could an attacker find a way to change a property of the security context (e.g., the document's CSP) *after* `WillFollowRedirect` has performed its checks but *before* the redirect is actually followed by the network service?
*   **CNAME Check Evasion**: The logic for checking DNS aliases assumes that the list of aliases provided by the network service is complete. A vulnerability in how the network service resolves DNS could potentially lead to an incomplete alias list, allowing a CNAME-cloaking bypass.
*   **Data URL Parsing**: The `HandleDataUrl` method parses and decodes `data:` URLs directly within the renderer process. Fuzzing this parsing logic could uncover vulnerabilities that would be exploitable by a malicious `data:` URL.

In summary, the `ResourceLoader` is the renderer's frontline soldier for a single network request. It is the component that most directly interacts with the `URLLoader` in the network service and is responsible for applying critical, context-sensitive security policies like CSP and `nosniff` to the request and response. Its correctness is essential for protecting the renderer from malicious network responses.