# HttpStreamParser (`net/http/http_stream_parser.cc`)

## 1. Summary

`HttpStreamParser` is a fundamental component of Chromium's network stack. It is the low-level engine responsible for serializing HTTP/1.x requests and, more importantly, parsing the responses received from a server. It directly handles reading from the socket and interpreting the byte stream into structured headers and a message body.

Given its position at the boundary between the network and the browser's higher-level logic, its correctness is absolutely critical for security. Flaws in this parser could lead to severe vulnerabilities like **HTTP Request/Response Smuggling**.

## 2. Core Concepts

The parser operates as a state machine (`DoLoop`) to handle the asynchronous nature of network I/O. Its primary responsibilities include:

*   **Sending Requests:** It formats and writes the request line, headers, and body (if any) to the socket.
*   **Header Parsing:** It reads from the socket until it finds the end-of-headers marker (`\r\n\r\n`).
*   **Body Framing:** After parsing headers, it determines how the response body is framed, based on these rules (in order of precedence):
    1.  **Chunked Encoding:** If `Transfer-Encoding: chunked` is present, it uses `HttpChunkedDecoder` to read the body.
    2.  **Content-Length:** If a `Content-Length` header is present, it reads exactly that many bytes.
    3.  **Connection Close:** If neither is present, it reads until the server closes the connection.
    4.  **No Body:** For certain status codes (e.g., 204, 304) or HEAD requests, it knows there is no body.
*   **Keep-Alive:** It determines if the connection can be reused (`CanReuseConnection`) based on response headers and whether any unexpected extra data was received.

## 3. Security-Critical Logic & Vulnerabilities

This class is the primary line of defense against HTTP Desynchronization (Response Smuggling) attacks. The code contains several explicit mitigations.

*   **Multiple Content-Length Headers:**
    *   **Vulnerability:** A classic smuggling technique involves sending two different `Content-Length` headers. A front-end proxy might process the first one, while the back-end browser processes the second, leading to a desynchronization of the request/response stream.
    *   **Mitigation (`ParseResponseHeaders`):** The parser explicitly checks for this condition and returns `ERR_RESPONSE_HEADERS_MULTIPLE_CONTENT_LENGTH` if multiple, distinct `Content-Length` headers are found.

*   **Truncated Headers over Secure Connections:**
    *   **Vulnerability:** An attacker could perform a MitM attack and truncate a response, hoping the browser misinterprets the partial data as a complete, different response (e.g., making an HTTP/1.1 response look like a short HTTP/0.9 response).
    *   **Mitigation (`HandleReadHeaderResult`):** The code contains a critical security check that makes this a fatal error if the connection is over a secure scheme (HTTPS/WSS).
        ```cpp
        // Accepting truncated headers over HTTPS is a potential security
        // vulnerability, so just return an error in that case.
        if (url_.SchemeIsCryptographic()) {
          io_state_ = STATE_DONE;
          return ERR_RESPONSE_HEADERS_TRUNCATED;
        }
        ```

*   **Incomplete Responses:**
    *   **Vulnerability:** An attacker could close a connection prematurely, hoping the browser accepts a partial body as complete, which could be used to hide malicious content from security scanners or trick the browser's logic.
    *   **Mitigation (`DoReadBodyComplete`):** The parser cross-references the bytes read against the expected framing information. If the connection is closed before the final chunk in a chunked response is seen, it returns `ERR_INCOMPLETE_CHUNKED_ENCODING`. If it's closed before `Content-Length` bytes are received, it returns `ERR_CONTENT_LENGTH_MISMATCH`.

*   **Extra Data & Socket Reuse:**
    *   **Vulnerability:** If a server sends extra data after the response body, and the socket is reused, that data could be prepended to the *next* response, leading to cross-user data leakage or smuggling.
    *   **Mitigation (`CanReuseConnection`):** The parser checks if any extra data was buffered after the response was considered complete. If so, it marks the connection as not reusable, forcing it to be closed.

## 4. Key Functions

*   `FindAndParseResponseHeaders()`: The core logic that scans the input buffer for the end-of-headers marker and invokes the header parser.
*   `HandleReadHeaderResult()`: Contains the crucial security check for truncated headers on HTTPS connections.
*   `DoReadBodyComplete()`: Contains the logic for detecting incomplete bodies and handling leftover data.
*   `CanReuseConnection()`: Determines if the connection is safe to be returned to the connection pool.

## 5. Related Files

*   `net/http/http_chunked_decoder.cc`: Helper class that encapsulates the logic for parsing chunked-encoded data.
*   `net/http/http_response_headers.cc`: Represents the parsed headers.
*   `net/http/http_util.cc`: Provides header parsing utility functions, including checks for multiple header instances.