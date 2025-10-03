# Security Notes for `services/network/public/mojom/url_loader.mojom`

This Mojo interface file defines the fundamental contract for performing network requests in Chromium. It establishes the communication channel between a client that wants to fetch a resource (e.g., a renderer process) and the `URLLoader` in the network service that executes the request. As the primary API for network data transfer, this interface is a critical security boundary.

## Interface Design and Security Philosophy

The `url_loader.mojom` interface is designed around a clear separation of roles:

*   **`URLLoader`**: An interface implemented by the network service. It represents a single, ongoing network request. The client holds a remote to this interface to control the request (e.g., follow redirects).
*   **`URLLoaderClient`**: An interface implemented by the client (e.g., the renderer). It receives notifications and data from the network service about the request's progress (e.g., redirects, response headers, completion).

A key security principle evident in this design is the **separation of mechanism and policy**. This Mojo interface primarily defines the *mechanism* for fetching a URL and communicating the results. The critical security *policies* (like CORS, CORP, CSP, mixed content blocking) are generally enforced by the creator of the `URLLoader` (the `URLLoaderFactory`) and within the `URLLoader` implementation itself, not by the client. The client is informed of the outcome of these policies (e.g., a blocked request results in an `OnComplete` call with an error) but is not trusted to enforce them.

## Security-Critical Methods and Parameters

### `FollowRedirect`

This is the most significant security control point within the interface itself. When the network service encounters a redirect, it pauses the request and notifies the client via `OnReceiveRedirect`. The client must then explicitly call `FollowRedirect` to continue.

*   **Security Boundary**: This pause gives the client (Blink in the renderer) a chance to apply security policies. For example, on a cross-origin redirect, Blink's logic is responsible for crafting the `removed_headers` and `modified_headers` to strip sensitive information like the `Authorization` header, preventing credentials from leaking to the destination server. A bug in this client-side logic could lead to a serious information disclosure vulnerability.
*   **`modified_cors_exempt_headers`**: This parameter is particularly sensitive as it allows for the modification of headers that are normally exempt from CORS checks. Its use must be tightly controlled to prevent security bypasses.
*   **`new_url` parameter**: This allows the client to substitute the redirect destination with a different URL. The interface mandates that this `new_url` **must be same-origin** with the original redirect destination. A bypass of this same-origin check in the `URLLoader` implementation would be a critical vulnerability, allowing an attacker to pivot a request to an arbitrary origin while potentially retaining the privileges (e.g., cookies) of the original request.

### `OnReceiveResponse`

*   **`body` (Data Pipe)**: The response body is streamed to the client via a Mojo data pipe. This is efficient but creates an asynchronous contract. The comments explicitly warn that the client must not discard the data pipe handle until `OnComplete` is called, even if it knows the body is empty. A failure to adhere to this contract could lead to the request being prematurely cancelled, which could have security consequences if, for example, a partially loaded script is executed or a security-critical response is not fully processed.

### `OnComplete`

*   **`URLLoaderCompletionStatus`**: This struct contains the final status of the request, including the error code. It's the primary way that the results of security policy checks (like CORS failures) are communicated to the client. The `ssl_info` field is only populated if explicitly requested, which is a good security practice to avoid leaking sensitive SSL details by default.

## Potential Attack Surface and Research Areas

*   **Redirect Logic Exploitation**: The most fruitful area for research is in finding ways to bypass the security logic that the client applies before calling `FollowRedirect`. Could a specific sequence of redirects or a malformed `URLRequestRedirectInfo` struct cause the client to misjudge the redirect's nature and fail to strip a sensitive header?
*   **State Machine Fuzzing**: The `URLLoader` and `URLLoaderClient` pair form a complex state machine. Fuzzing the interface with unexpected call sequences (e.g., calling `FollowRedirect` twice, or calling it without receiving `OnReceiveRedirect`) could uncover state confusion vulnerabilities in the network service.
*   **Bypassing the `new_url` Same-Origin Check**: Any vulnerability in the `URLLoader`'s implementation of the same-origin check for the `new_url` in `FollowRedirect` would be a high-severity bug.
*   **Information Leaks in `URLResponseHead`**: Fuzzing the parsing of the `URLResponseHead` struct in the client process could reveal vulnerabilities. While the network service is generally trusted, a compromised network service could exploit such a bug to attack other browser processes.

In summary, `url_loader.mojom` is a well-defined interface that serves as the backbone of Chromium's network stack. Its security relies on the client correctly implementing security policies during redirects and the `URLLoader` robustly validating any instructions it receives from the client.