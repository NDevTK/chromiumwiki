# InsecureCredentialsManager (`components/password_manager/core/browser/ui/insecure_credentials_manager.cc`)

## 1. Summary

The `InsecureCredentialsManager` is a crucial component that acts as the source of truth for the state of all insecure credentials within a profile. It is responsible for three main tasks:
1.  Persisting the results of server-side security checks (i.e., saving the fact that a credential is leaked or phished).
2.  Triggering and caching the results of expensive client-side security checks (i.e., identifying weak and reused passwords).
3.  Synthesizing these different issue types into a single, comprehensive list of insecure credentials for consumption by UI components like the Password Checkup page.

It sits between the `SavedPasswordsPresenter` (which provides the raw list of saved passwords) and UI-facing handlers (like `PasswordCheckDelegate`), abstracting away the complexity of how insecurity is determined and stored.

## 2. Architecture: A Caching and Synthesizing Layer

The manager's design is that of a smart cache and data synthesizer. It doesn't have its own persistent storage, but rather orchestrates the storage of insecurity information in the `PasswordStore` and caches the results of local computations.

*   **Data Source**: The manager's ground truth is the `SavedPasswordsPresenter`, which it observes for any changes to the user's saved passwords.

*   **Asynchronous Checks**: For performance, the expensive checks for weak and reused passwords are not run on the UI thread.
    *   `StartWeakCheck` and `StartReuseCheck` post a task to a background thread pool.
    *   The heavy lifting is done by the `BulkWeakCheck` and `BulkReuseCheck` functions, which operate on the provided list of credentials.
    *   The results (sets of weak or reused passwords) are returned to the UI thread and stored in the `weak_passwords_` and `reused_passwords_` member variables, which act as a cache.

*   **Persisting Leaked Credentials**: The `SaveInsecureCredential` method is the key entry point for making leak information permanent. It takes a leaked credential, finds the corresponding saved entry in the `SavedPasswordsPresenter`, and updates its `password_issues` map. This change is then sent back through the presenter to the `PasswordStore`, where it is written to the database.

*   **Synthesizing Results**: The main getter, `GetInsecureCredentialEntries`, dynamically creates the list of insecure credentials. It iterates through all passwords from the presenter and, for each one, combines the persisted issues (like `kLeaked` from the `PasswordForm` itself) with the cached, locally-computed issues (by checking if the password is in `weak_passwords_` or `reused_passwords_`).

## 3. Security-Critical Logic

The security of the Password Checkup feature relies heavily on the correctness of this manager's logic.

*   **`SaveInsecureCredential`**: This is the most critical function. It is responsible for permanently flagging a credential as leaked.
    *   **Matching Logic**: It correctly uses `CanonicalizeUsername` to match the incoming leaked credential against the stored credentials. This is vital to ensure that variations in username capitalization or domain (e.g., `gmail.com` vs `googlemail.com`) don't cause a leak to be missed.
    *   **Modification**: It directly modifies the `password_issues` map of the `CredentialUIEntry` before calling `presenter_->EditSavedCredentials()`. A bug here, such as modifying the wrong field or using the wrong `InsecureType`, would break the feature.

*   **`MuteCredential` / `UnmuteCredential`**: These methods modify the `is_muted` flag within the `password_issues` map for a given credential.
    *   **Targeted Logic**: The logic correctly uses a `SupportsMuteOperation` check to ensure that only server-verifiable issues (`kLeaked`, `kPhished`) can be muted. Client-side issues like `kWeak` cannot be muted, which is the intended behavior.
    *   **Integrity**: Like saving, this relies on the `EditSavedCredentials` flow to persist the state change, ensuring the data remains consistent in the `PasswordStore`.

*   **`OnSavedPasswordsChanged`**: This observer method is critical for data consistency.
    *   When passwords are changed, it intelligently triggers new weak and reuse checks *only* for the affected credentials, rather than re-running the entire expensive check. This is both a performance optimization and a correctness feature, ensuring the cached data doesn't become stale.
    *   A failure to correctly identify which changes require a re-check could lead to the UI showing outdated insecurity information.

## 4. Related Components

*   `components/password_manager/core/browser/ui/saved_passwords_presenter.h`: The source of truth for all saved passwords. The manager is tightly coupled with it.
*   `components/password_manager/core/browser/password_store.h`: The ultimate destination for the `password_issues` data that this manager modifies.
*   `chrome/browser/extensions/api/passwords_private/password_check_delegate.h`: The primary client of this class, which uses it to drive the Password Checkup UI.
*   `components/password_manager/core/browser/ui/reuse_check_utility.h` & `weak_check_utility.h`: These files contain the actual logic for identifying reused and weak passwords, respectively, which is run on the background thread.