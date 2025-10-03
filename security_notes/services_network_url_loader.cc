# Security Notes for `services/network/url_loader.cc`

The `URLLoader` class is the workhorse of Chromium's network stack. It is the concrete implementation of the `mojom::URLLoader` interface, responsible for managing the entire lifecycle of a single network request. It translates the high-level `ResourceRequest` into a `net::URLRequest`, orchestrates its execution, and enforces numerous security policies along the way. Its complexity and central role make it a highly security-sensitive component.

## Lifecycle and State Machine

A `URLLoader` instance represents a complex state machine that progresses through several stages:

1.  **Initialization**: The loader is created with a `ResourceRequest` and sets up a `net::URLRequest`, configuring it with parameters like the URL, method, headers, and load flags.
2.  **Interception and Start**: Before the request is sent, it may be intercepted by various helpers for features like Trust Tokens or Shared Storage. It is then scheduled for execution.
3.  **Redirects**: If the server responds with a redirect (`3xx` status code), the loader pauses and calls `OnReceiveRedirect` on its `URLLoaderClient`. It waits for the client to call `FollowRedirect` before proceeding.
4.  **Authentication**: If the server requires authentication (`401` or `407`), the loader pauses and delegates the credential-gathering process to a privileged observer (in the browser process).
5.  **Response**: Once headers are received (`OnResponseStarted`), the loader performs critical security checks (CORP, ORB, etc.) before streaming the body to the client.
6.  **Completion**: The loader sends a final `OnComplete` message to the client when the request finishes, either successfully or with an error.

The correctness of this state machine is critical; state confusion vulnerabilities could lead to security checks being bypassed.

## Core Security Enforcement Mechanisms

The `URLLoader` is a primary point of enforcement for many of the web's security models.

*   **Redirect Security (`FollowRedirect`)**: This is one of the most critical security functions in the class.
    *   When the client calls `FollowRedirect`, the loader must re-evaluate the security context. It resets security-sensitive headers (like `Cookie`), re-applies `Sec-Fetch-*` headers, and re-evaluates the `site_for_cookies` and Storage Access API status.
    *   **`new_url` Same-Origin Check**: The implementation must strictly enforce that the optional `new_url` parameter is same-origin with the original redirect destination. A bypass here would be a severe vulnerability, allowing a request to be retargeted to an arbitrary origin while potentially carrying privileges from the redirect chain.

*   **Response Blocking Policies**: `OnResponseStarted` is a major security gate.
    *   **Opaque Response Blocking (ORB)**: The `orb_analyzer_` inspects the response to prevent cross-site "no-cors" requests from leaking data into a renderer process. If ORB decides to block a response, `BlockResponseForOrb` is called, which sanitizes the headers, sends an empty body to the client, and **crucially, kills the underlying network connection** to prevent data from being leaked via side channels.
    *   **Cross-Origin Resource Policy (CORP)**: `CrossOriginResourcePolicy::IsBlocked` is called here to enforce the `Cross-Origin-Resource-Policy` header, preventing a document from loading resources that don't explicitly permit it.

*   **Private Network Access (PNA)**: The `private_network_access_interceptor_` is invoked at the `OnConnected` stage. This enforces the PNA specification, preventing public websites from making requests to private IP addresses and localhosts, which is a powerful defense against CSRF attacks on local network devices (e.g., routers).

*   **Header Sanitization and Validation**: The loader is responsible for ensuring that clients (renderers) cannot set privileged, "forbidden" headers. The `AreRequestHeadersSafe` check is used to validate headers modified by the client during redirects.

*   **Delegation of Privileged Operations**: Sensitive operations are not performed directly in the `URLLoader`. Instead, they are delegated to a trusted client in the browser process via Mojo observers:
    *   `OnAuthRequired` delegates HTTP authentication to the `URLLoaderNetworkServiceObserver`.
    *   `OnCertificateRequested` delegates client certificate selection.
    *   This follows the principle of least privilege, as the (potentially compromised) renderer process never handles raw credentials or has access to the system's certificate store.

## Potential Attack Surface and Research Areas

*   **State Machine Fuzzing**: The complex interplay between network events (`OnResponseStarted`, `OnReceivedRedirect`) and client calls (`FollowRedirect`) is a prime target for fuzzing. Attempting to trigger these events out of order or with unexpected parameters could uncover state confusion vulnerabilities that bypass security checks.
*   **Redirect Logic Bypasses**: Exploiting edge cases in URL parsing or origin serialization to fool the `new_url` same-origin check in `FollowRedirect`. Another avenue is to find a redirect sequence that causes the loader to fail to properly reset the security context (e.g., not stripping a sensitive header).
*   **Interceptor Interaction Bugs**: The request lifecycle involves a chain of interceptors (Trust Tokens, Shared Storage, PNA, ORB). A logic flaw in how these interceptors interact or how their results are combined could lead to a security check being skipped. For example, does an error from an early interceptor correctly terminate the request, or could it cause a later, more critical interceptor like ORB to be bypassed?
*   **Lifetime and Memory Safety**: The `URLLoader`'s lifetime is managed by its `delete_callback_` and its Mojo connection. A race condition between a network callback and a Mojo disconnection could lead to a use-after-free. The code uses `weak_ptr_factory_` to mitigate this, but any raw pointers or `base::Unretained` usage in callbacks are areas of concern.

In summary, `URLLoader` is a central and highly complex class where request-time security policies are enforced. Its security relies on the robust implementation of its state machine, the correct application of various security interceptors, and safe handling of its asynchronous, event-driven lifecycle.