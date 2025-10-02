# PasswordStore (`components/password_manager/core/browser/password_store/password_store.cc`)

## 1. Summary

The `PasswordStore` class is the central, thread-safe, asynchronous frontend for all password database operations in Chromium. It lives on the main UI thread and acts as the primary API for any browser component that needs to create, retrieve, update, or delete user credentials. It does not perform any storage operations itself; instead, it serves as a crucial gatekeeper that marshals requests to the `PasswordStoreBackend`, which executes them on a background thread. This design prevents blocking the UI thread with slow I/O and provides a single, controlled point of entry for security-sensitive password data.

## 2. Architecture: A Thread-Safe Facade

The core architectural pattern of the `PasswordStore` is the separation of the public-facing API from the implementation details of the storage mechanism.

*   **UI Thread (Frontend)**: The `PasswordStore` object itself lives on the main browser thread. It exposes a clean, asynchronous API (e.g., `AddLogin`, `GetLogins`) to its clients, such as the `PasswordFormManager`. All its public methods are expected to be called on this thread.

*   **Background Thread (Backend)**: All actual I/O operations (reading from or writing to the database) are delegated to a `PasswordStoreBackend` object. The `PasswordStore` posts tasks to the backend, which runs on a dedicated background thread to avoid jank.

*   **Asynchronous Communication**: Communication is handled via callbacks.
    *   Requests from the UI thread to the backend are made via `...Async` methods (e.g., `backend_->AddLoginAsync(...)`).
    *   Results are returned from the backend to the UI thread using callbacks, typically wrapped in `base::BindPostTask` to ensure they are executed safely back on the main thread.
    *   For retrieving data, the `PasswordStoreConsumer` class is used with a `base::WeakPtr` to safely handle cases where the requester might be destroyed before the data is ready.

This asynchronous, multi-threaded design is fundamental to the security and responsiveness of the password manager.

## 3. Security-Critical Logic & Operations

*   **`UpdateLoginWithPrimaryKey`**: This is one of the most security-sensitive methods. It replaces an old credential with a new one. The implementation contains critical logic to sanitize the security metadata (`password_issues`) of the credential:
    *   If the password value changes, all `password_issues` (leaked, phished, weak, etc.) are cleared. This is correct because the old issues are no longer relevant to the new password.
    *   If only the username changes, the leaked and phished flags are cleared, as these are tied to the username-password pair.
    *   A bug in this logic could cause a user who just fixed a leaked password to continue seeing a "leaked password" warning, eroding trust in the feature.

*   **`AddLogins` / `UpdateLogins` Sanity Check**: These methods include a `CHECK` to ensure that any `PasswordForm` marked as `blocked_by_user` (i.e., a "never save for this site" entry) does not also contain a username and password. This is a vital sanity check to prevent a logic error from accidentally storing a credential in a blocklist entry.

*   **Backend Abstraction (`PasswordStoreBackend`)**: The `PasswordStore`'s security model heavily relies on its backend. By abstracting the backend, Chromium can use the most secure storage mechanism available on each platform:
    *   **macOS**: Keychain
    *   **Windows**: DPAPI / Credential Locker
    *   **Linux**: GNOME Keyring / KWallet
    *   **Android**: Google Password Manager service
    This abstraction ensures that plaintext passwords are never stored directly on disk and are protected by OS-level encryption and security measures.

*   **Shutdown and Cleanup (`ShutdownOnUIThread`)**: The class has a careful shutdown procedure that is essential for preventing use-after-free vulnerabilities. It transfers ownership of the `backend_` to the shutdown task and nulls out its local pointer, ensuring no further requests can be made to a partially destroyed object.

## 4. Attack Surface & Risks

*   **Race Conditions**: The asynchronous, multi-threaded nature of the store makes it susceptible to race conditions. A logic error in how callbacks are chained or how state is managed between the two threads could lead to data corruption (e.g., an old password overwriting a new one). The code uses `base::BarrierCallback` to mitigate this for multi-step operations, ensuring all prerequisite tasks are finished before the final callback is invoked.

*   **Information Leaks**: The primary risk is the accidental exposure of `PasswordForm` data. While the `PasswordStore` itself is careful, any component that interacts with it receives `PasswordForm` objects. A bug in a consumer could lead to plaintext credentials being logged or sent over the network.

*   **Incorrect State Management**: The `PasswordStore` handles requests that arrive before its backend is fully initialized by queueing them in a `post_init_callback_`. A flaw in this initialization sequence could cause early requests to be dropped or handled incorrectly, potentially leading to a user's password not being saved.

## 5. Related Components

*   `components/password_manager/core/browser/password_store/password_store.h`: The header file defining the `PasswordStore` interface.
*   `components/password_manager/core/browser/password_store/password_store_backend.h`: The interface for the platform-specific backend that performs the actual storage operations.
*   `components/password_manager/core/browser/password_form.h`: The data structure representing a single credential.
*   `components/password_manager/core/browser/password_store/password_store_consumer.h`: The interface for clients that wish to asynchronously retrieve results from the `PasswordStore`.