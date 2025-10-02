# Password Bulk Leak Check (`components/password_manager/core/browser/leak_detection/bulk_leak_check_service.cc`)

## 1. Summary

The `BulkLeakCheckService` is the central, profile-specific (`KeyedService`) orchestrator for the Password Checkup feature. Its purpose is to manage the process of checking a large volume of saved passwords against the leak database in a controlled and observable manner. It acts as a state machine and a delegation point, but does not perform any cryptographic operations or network requests itself. Instead, it creates and manages a `BulkLeakCheck` object, which is responsible for the actual work.

## 2. Architecture: A State-Driven Orchestrator

The service's architecture is designed to manage a potentially long-running and resource-intensive operation while providing clear state updates to its clients (primarily the Password Checkup UI).

1.  **Initiation**: The process begins when a client calls `CheckUsernamePasswordPairs`, providing a list of `LeakCheckCredential` objects.

2.  **State Management**: The service immediately transitions its internal state to `kRunning` and notifies its observers. It maintains a simple but crucial state machine with states like `kIdle`, `kRunning`, `kCanceled`, and various error states (`kSignedOut`, `kNetworkError`, etc.).

3.  **Delegation to `BulkLeakCheck`**: The service's primary action is to create a `BulkLeakCheck` object (via `LeakDetectionCheckFactoryImpl`). This `BulkLeakCheck` object is the true worker: it takes the list of credentials and is responsible for iterating through them, creating a `LeakDetectionCheck` for each one, and executing the check. If a check is already in progress when `CheckUsernamePasswordPairs` is called again, the new credentials are simply appended to the queue of the existing `BulkLeakCheck` instance.

4.  **Asynchronous Results**: The `BulkLeakCheckService` implements the `BulkLeakCheckDelegateInterface`. The `BulkLeakCheck` object reports its progress back to the service through this interface:
    *   `OnFinishedCredential`: This callback is invoked for each credential as its check completes, providing the result (`IsLeaked`). The service then forwards this result to its own observers.
    *   `OnError`: If a fatal error occurs during the process (e.g., network failure, authentication token couldn't be obtained), this method is called. The service translates the specific `LeakDetectionError` into the appropriate public state and terminates the check.

5.  **Completion**: When the `BulkLeakCheck` has processed all its credentials (i.e., `GetPendingChecksCount()` returns 0), the `BulkLeakCheckService` transitions its state back to `kIdle`, cleans up the `bulk_leak_check_` object, and notifies observers that the process is complete.

## 3. Security Considerations

The security posture of `BulkLeakCheckService` is primarily about **correct orchestration and state management**, rather than direct data handling.

*   **Reliance on `LeakDetectionCheck`**: The service's security and privacy guarantees are entirely inherited from the underlying `LeakDetectionCheck` component. It trusts that each individual check is performed using the privacy-preserving cryptographic protocol. The `BulkLeakCheckService` itself never handles plaintext passwords; it only marshals `LeakCheckCredential` objects to the worker class.

*   **State Integrity**: The most critical security responsibility of this class is maintaining the integrity of its state machine. A bug that caused the state to be reported incorrectly could confuse the user or UI logic. For example, if it reported `kIdle` prematurely, the user might believe their check was complete when it was not. The implementation correctly handles transitions upon start, completion, cancellation (`Cancel` method), and errors.

*   **Resource Management**: The service correctly owns and manages the lifecycle of the `bulk_leak_check_` object. The `Cancel` and `Shutdown` methods ensure that the underlying check is stopped and resources are released, preventing orphaned processes or use-after-free conditions.

*   **Error Propagation**: It correctly translates low-level `LeakDetectionError` types into high-level service states. This ensures that a critical failure (like `kTokenRequestFailure`) is not silently ignored but is instead propagated to the UI layer, which can then prompt the user appropriately.

## 4. The Worker: `BulkLeakCheckImpl`

The `BulkLeakCheckImpl` class is the concrete implementation of the `BulkLeakCheck` interface and acts as the true "worker" for the bulk check process. It receives the list of credentials from the service and manages their journey through the leak check pipeline.

*   **Pipeline Architecture**: The class's core design is a set of queues that represent the stages of the leak check process for a credential. This architecture serves as a natural throttling mechanism, preventing an overwhelming number of concurrent operations. The queues are:
    1.  `waiting_encryption_`: Credentials that have been received but are waiting for their cryptographic payload to be computed on a background thread.
    2.  `waiting_token_`: Credentials that have a payload but are waiting for an OAuth2 access token.
    3.  `waiting_response_`: Credentials with a payload and token that are waiting for a response from the leak detection API.
    4.  `waiting_decryption_`: Credentials that have a server response and are waiting for the final client-side analysis.

*   **Processing Flow**:
    1.  When `CheckCredentials` is called, all credentials are put into the `waiting_encryption_` queue, and a task is posted to a background thread to compute the payload for each one using `PrepareSingleLeakRequestData`. A single, secret `encryption_key_` is generated once for the entire lifetime of the `BulkLeakCheckImpl` object and is used for all credentials in the batch.
    2.  As each payload is prepared, the corresponding credential moves to the `waiting_token_` queue, and an access token request is immediately fired.
    3.  When a token is received, the credential moves to `waiting_response_`, and the `LeakDetectionRequest` is sent.
    4.  When the network response arrives, the credential moves to `waiting_decryption_` for the final `AnalyzeResponse` step.
    5.  Once the analysis is complete, the final result is reported to the delegate (`BulkLeakCheckService`), and the credential is removed from the pipeline.

*   **Security & Robustness**:
    *   **Throttling**: This pipeline design is inherently self-throttling. The number of concurrent network requests is limited by the speed of the preceding cryptographic and token-request steps.
    *   **Fail-Fast Error Handling**: If any step in the pipeline fails for a given credential (e.g., token request fails, network error), the entire bulk check is aborted by calling `delegate_->OnError()`. This is a robust design that prevents the system from continuing in a partially failed state.
    *   **Separation of Concerns**: The class correctly delegates the sensitive cryptographic work to `PrepareSingleLeakRequestData` and `AnalyzeResponse`, focusing its own logic on state management and flow control.

## 5. Related Components

*   `components/password_manager/core/browser/leak_detection/bulk_leak_check_service.h`: The owner and public-facing interface for the bulk leak check feature.
*   `components/password_manager/core/browser/leak_detection/leak_detection_check_factory_impl.h`: The factory used to instantiate the `BulkLeakCheckImpl` object.
*   `chrome/browser/ui/webui/settings/password_check_handler.h`: A likely client of this service, responsible for bridging the gap between the C++ backend and the `chrome://settings` UI.