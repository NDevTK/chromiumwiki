# SavedPasswordsPresenter (`components/password_manager/core/browser/ui/saved_passwords_presenter.cc`)

## 1. Summary

The `SavedPasswordsPresenter` is a high-level data management class that acts as the primary source of truth for all saved credentials (both passwords and passkeys) presented in the Chrome UI. It is a critical component that sits between the low-level storage (`PasswordStore`, `PasskeyModel`) and the UI logic layers (`PasswordCheckDelegate`, `InsecureCredentialsManager`). Its core responsibilities are to fetch raw credential data from multiple sources, process it into a unified and user-friendly format, and provide a stable, observable interface for its clients to read and modify this data.

## 2. Architecture: A Unifying Data Abstraction Layer

The presenter's architecture is designed to abstract away the complexity of Chromium's multi-store and multi-type credential system.

1.  **Multi-Source Fetching**: Upon initialization (`Init`), the presenter asynchronously fetches all credentials from up to three sources:
    *   `ProfilePasswordStore`: For device-local passwords.
    *   `AccountPasswordStore`: For passwords synced to the user's Google Account.
    *   `PasskeyModel`: For all passkeys (WebAuthn credentials).
    It uses a counter (`pending_store_updates_`) to track when all asynchronous fetches are complete.

2.  **Data Aggregation and Grouping**:
    *   Raw `PasswordForm` objects are stored in an internal map (`sort_key_to_password_forms_`).
    *   The presenter's key architectural feature is its use of a `PasswordsGrouper`. Once all data is fetched, it passes the complete list of passwords and passkeys to the grouper.
    *   The `PasswordsGrouper` performs the complex logic of combining credentials from different sources based on affiliation data (e.g., grouping `google.com` and `youtube.com` together). The output of this process is a list of `CredentialUIEntry` objects, which is the unified data model that clients interact with.

3.  **Observable Interface**: The presenter implements the Observer pattern.
    *   It observes the underlying `PasswordStore` and `PasskeyModel` for any real-time changes.
    *   When changes occur (`OnLoginsChanged`, `OnPasskeysChanged`), it updates its internal cache and re-runs the grouping logic.
    *   It then notifies its own observers (like `PasswordCheckDelegate`) that the list of saved credentials has changed, ensuring the entire UI layer remains in sync.

## 3. Security-Critical Logic

As the gatekeeper for all modifications to the user's saved credentials, the presenter contains highly security-sensitive logic.

*   **`EditPassword` - The Core Modification Logic**: This is the most critical function in the class. It is called when a user edits a password in the settings UI.
    *   **Conflict Detection**: Before applying any change, it performs a crucial validation check: `IsUsernameAlreadyUsed`. This ensures that a user cannot change a credential's username to one that already exists for the same site in the same store, which would effectively overwrite a different credential. This is a primary defense against data corruption.
    *   **Correct API Usage**: The function correctly distinguishes between changing a password and changing a username. When a username is changed (which alters the primary key of the `PasswordForm`), it correctly calls the `PasswordStore::UpdateLoginWithPrimaryKey` method. For all other changes, it uses the standard `PasswordStore::UpdateLogin`. Using the wrong API could lead to orphaned or duplicated entries.
    *   **Password Issue Invalidation**: The logic correctly understands the security implications of editing a password. When the password value is changed, it clears *all* `password_issues` associated with the credential, correctly assuming that the old leak/weakness/reuse status is no longer valid for the new password.

*   **`MoveCredentialsToAccount`**: This function handles the logic for moving a password from the local profile store to the synced account store. Its security relies on correctly checking if a credential for the same `signon_realm` already exists in the account store before adding the new one and deleting the old one. This prevents accidental duplication or overwrites.

*   **`RemoveCredential`**: This function correctly identifies all `PasswordForm` objects associated with a given `CredentialUIEntry` (since a single UI entry can represent multiple forms due to grouping) and calls `RemoveLogin` on the appropriate store for each one. This ensures that when a user deletes a credential, all underlying database entries are properly removed.

## 4. Related Components

*   `components/password_manager/core/browser/ui/passwords_grouper.h`: The helper class responsible for the complex logic of grouping affiliated credentials.
*   `components/password_manager/core/browser/password_store.h`: The underlying storage interface for passwords. The presenter is its primary "write" client for UI-initiated changes.
*   `components/webauthn/core/browser/passkey_model.h`: The underlying storage interface for passkeys.
*   `chrome/browser/extensions/api/passwords_private/password_check_delegate.h`: A primary client of the presenter, which uses it as the source of truth for the Password Checkup feature.