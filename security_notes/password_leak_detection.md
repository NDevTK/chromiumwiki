# Password Leak Detection (`components/password_manager/core/browser/leak_detection/`)

## 1. Summary

The Password Leak Detection feature is a highly sophisticated privacy-preserving mechanism designed to warn users if their credentials have appeared in a known data breach. The implementation, centered around `LeakDetectionCheckImpl`, orchestrates a complex cryptographic protocol to query a Google-owned database of leaked credentials. The core security guarantee is that **plaintext usernames and passwords are never sent to Google**. Instead, the client performs a "blinded" query, ensuring that Google cannot learn which credential is being checked, and the user's privacy is maintained.

## 2. Architecture: A Parallelized Cryptographic Flow

The process of checking a single credential (`username`, `password` pair) is managed by `LeakDetectionCheckImpl` and follows a carefully parallelized flow to minimize latency:

1.  **Start**: The check begins with `LeakDetectionCheckImpl::Start()`.

2.  **Parallel Operations**: A helper class, `RequestPayloadHelper`, immediately kicks off two operations in parallel:
    *   **Authentication**:
        *   If the user is signed into a Google account, it requests an OAuth2 access token via `RequestAccessToken`. This token is used to authenticate the request to Google's servers.
        *   If the user is not signed in, it proceeds without a token, relying on a less-privileged API key for the request.
    *   **Payload Preparation**:
        *   Simultaneously, it calls `PrepareSingleLeakRequestData` to perform the client-side cryptography. This is the most security-sensitive part of the request pipeline. It takes the plaintext username and password and transforms them into a blinded, encrypted query payload. A temporary, single-use `encryption_key` is also generated during this step, which is kept exclusively on the client.

3.  **Synchronization & Network Request**:
    *   The `RequestPayloadHelper` waits for both the auth token (if applicable) and the cryptographic payload to be ready.
    *   Once both are available, it calls `LeakDetectionCheckImpl::DoLeakRequest`.
    *   `DoLeakRequest` creates a `LeakDetectionRequest` object, which constructs and sends the final HTTPS request to Google's servers. The request contains the blinded payload and the authentication token/API key.

4.  **Response Analysis**:
    *   The server's response is handled by `OnLookupSingleLeakResponse`. The response does not contain a simple "yes" or "no". Instead, it contains a list of `encrypted_leak_match_prefixes`.
    *   This encrypted data is passed to the `AnalyzeResponse` function, along with the secret `encryption_key` that was generated and stored on the client in step 2.
    *   `AnalyzeResponse` performs the final client-side cryptographic step: it uses the key to determine if the user's specific credential corresponds to any of the encrypted data returned by the server.

5.  **Result**: The final result (`kLeaked` or `kNotLeaked`) is passed to the `LeakDetectionDelegateInterface`, which is responsible for updating the `PasswordForm` in the database and triggering a user-facing notification.

## 3. Security-Critical Logic & Cryptographic Protocol

The entire security of this feature hinges on the cryptographic protocol that allows for a private lookup.

*   **Blinded Request (`PrepareSingleLeakRequestData`)**: The client does not send `hash(password)`. Instead, it computes a blinded version of the credential. The protocol likely involves:
    *   Computing a hash of the username to find a bucket of credentials on the server.
    *   Hashing the password multiple times, including with a server-side public key, to create a value that can be checked by the server without revealing the original password hash.
    *   Generating a client-side secret encryption key (`encryption_key_`) that will be needed to decrypt the server's response.

*   **Server-Side Lookup**: The server receives the blinded query. It finds all credentials associated with the (hashed) username prefix. It cannot identify the specific user or their password from the request. It returns a set of encrypted data that corresponds to the potential matches.

*   **Client-Side Analysis (`AnalyzeResponse`)**: This is the final, critical step. The client receives the encrypted data from the server. Using the secret `encryption_key_` it generated earlier, it can "un-blind" the response and check if its exact hashed password is in the set returned by the server. Because the client generated the key, only it can perform this final check. This ensures that Google never knows the final result of the lookup.

*   **Enterprise Policy (`IsURLBlockedByPolicy`)**: The feature includes a check against the `PasswordManagerLeakDetectionURLAllowlist` enterprise policy. This ensures that credentials for sensitive internal domains specified by a system administrator are never sent for a leak check, respecting enterprise security boundaries.

## 4. The Network Request (`LeakDetectionRequest`)

The `LeakDetectionRequest` class is responsible for the final step of the process: taking the cryptographically prepared payload, constructing an HTTPS request, sending it to the Google API endpoint, and parsing the response.

*   **Endpoint**: The request is sent to a hardcoded endpoint, `kLookupSingleLeakEndpoint`.

*   **Request Construction (`LookupSingleLeak`)**:
    *   **Authentication**: The request includes an `Authorization` header. For signed-in users, this is a `Bearer` token containing the OAuth2 access token. For signed-out users, it uses an `x-goog-api-key` header with a hardcoded API key.
    *   **Payload**: The `LookupSingleLeakPayload` (containing the blinded username/password data) is serialized into a `LookupSingleLeakRequest` protocol buffer. This binary data is sent as the `POST` body with a `Content-Type` of `application/x-protobuf`.
    *   **Metadata**: The request includes the `ClientUseCase` (e.g., `CHROME_SIGN_IN_CHECK`, `CHROME_BULK_SYNCED_PASSWORDS_CHECK`) to give the server context about why the check is being performed. It may also include a `RequestCriticality` header to help the server with load shedding.

*   **Response Handling (`OnLookupSingleLeakResponse`)**:
    *   The response handler robustly checks for network errors and HTTP status codes, specifically handling `429 Too Many Requests` to signal a quota limit issue back to the caller.
    *   On success, it parses the response body as a `LookupSingleLeakResponse` protobuf.
    *   It extracts the two key fields from the response: `reencrypted_lookup_hash` and the list of `encrypted_leak_match_prefix`.
    *   This data is packaged into a `SingleLookupResponse` struct and sent back to `LeakDetectionCheckImpl` for the final, client-side analysis.

This class acts as a clean boundary between the complex cryptographic setup and the standard networking layer. Its primary security responsibility is to ensure that the correct authentication headers and the correctly serialized, privacy-preserving payload are sent over a secure HTTPS channel.

## 5. Related Components

*   `components/password_manager/core/browser/leak_detection/leak_detection_request_utils.cc`: This file contains the core cryptographic logic (`PrepareSingleLeakRequestData` and `AnalyzeResponse`) and is the most important dependency for understanding the privacy guarantees.
*   `components/password_manager/core/browser/leak_detection/leak_detection_check_impl.cc`: The central orchestrator that drives the entire leak check process, from requesting an auth token to analyzing the final response.
*   `components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h`: The interface used by `LeakDetectionCheckImpl` to report its results (success, failure, or leak found) to the rest of the password manager.