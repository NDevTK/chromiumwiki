# Password Import and Export

## 1. Summary

Password import and export are highly sensitive operations that allow users to move their credentials into and out of the Chrome password store. The export process creates a plaintext `.csv` file of all passwords, representing a significant security risk if handled improperly. The import process parses a file and adds its contents to the password store, creating a risk of data corruption or malicious entry if the input is not handled carefully. The `PasswordsPrivateDelegateImpl` serves as the initial entry point for these operations from the UI, but it quickly delegates the core logic to a dedicated helper class, the `PasswordManagerPorter`.

## 2. The Export Flow

The export process is initiated from the settings UI and is designed with a critical security gate: user re-authentication.

*   **Initiation (`PasswordsPrivateDelegateImpl::ExportPasswords`)**:
    *   When the user clicks the "Export passwords" button in the UI, the `passwordsPrivate.exportPasswords()` API function is called, which is handled by this method in the delegate.
    *   The method's first and only action is to call `AuthenticateUser()`. This triggers a platform-specific OS-level authentication prompt (e.g., Touch ID on macOS, Windows Hello on Windows) to confirm the user's presence and authorization. This is a crucial step to prevent malware or an unauthorized physical user from silently exporting all passwords.

*   **Authentication Result (`OnExportPasswordsAuthResult`)**:
    *   If authentication is successful, this callback is invoked.
    *   It then immediately delegates the entire export process to `password_manager_porter_->Export()`.

*   **The Exporter (`PasswordManagerPorter`)**:
    *   The `PasswordManagerPorter` is the class that contains the actual logic for serializing the passwords and writing them to a file.
    *   It retrieves the full list of credentials from the `SavedPasswordsPresenter`.
    *   It then serializes this list into a standard CSV format with the columns `name,url,username,password,note`.
    *   It presents a native "Save As" file dialog to the user, allowing them to choose the destination for the plaintext `.csv` file.

*   **Security Considerations**:
    *   **Re-authentication is Mandatory**: The entire security of the export feature rests on the mandatory OS-level re-authentication step. This is a robust defense against non-interactive attacks.
    *   **Plaintext Risk**: The fundamental risk is the creation of a plaintext file containing all of the user's passwords. The UI provides warnings about the sensitivity of this file, but the ultimate security depends on the user's handling of the exported data.
    *   **Serialization**: The serialization logic is straightforward, but a bug could lead to a malformed CSV file. The use of a standard format mitigates the risk of creating a file that cannot be imported by other password managers.

## 3. The Import Flow

The import process is more complex than exporting, as it involves parsing an untrusted file and handling potential conflicts with the user's existing passwords.

*   **Initiation (`PasswordsPrivateDelegateImpl::ImportPasswords`)**:
    *   The flow begins when the user clicks the "Import passwords" button in the UI.
    *   The delegate calls `PasswordManagerPorter::Import`, which immediately opens a native file selection dialog for the user to choose a `.csv` file.

*   **Parsing and Conflict Resolution (`PasswordManagerPorter` & `PasswordImporter`)**:
    *   Once a file is selected, `PasswordManagerPorter` creates a `password_manager::PasswordImporter` object.
    *   The `PasswordImporter` is the "worker" class responsible for parsing the CSV file. It reads each row and attempts to convert it into a valid `PasswordForm`.
    *   Crucially, after parsing, the importer does not immediately save the passwords. Instead, it performs a **conflict analysis**. It compares each imported credential against the user's existing passwords (retrieved via the `SavedPasswordsPresenter`) to identify entries that would be overwritten.
    *   If conflicts are found, the import process **pauses** and returns a list of the conflicting entries to the UI layer (`PasswordsPrivateDelegateImpl`). The UI then displays a conflict resolution dialog, asking the user to select which specific passwords they want to save and which they want to skip.

*   **Continuation (`PasswordsPrivateDelegateImpl::ContinueImport`)**:
    *   After the user makes their selections in the UI, this method is called with the list of chosen credential IDs.
    *   It may require another OS-level re-authentication check to confirm the user's intent to modify their password store.
    *   If authenticated, it calls `password_manager_porter_->ContinueImport()`, passing along the user's selections.
    *   The `PasswordImporter` then performs the final step, calling `presenter_->AddCredential()` only for the user-approved entries.

*   **Security Considerations**:
    *   **Parsing Untrusted Input**: The primary risk is in parsing the `.csv` file. A malformed file could potentially lead to parsing errors, hangs, or even memory corruption bugs in the CSV parsing library.
    *   **Conflict Resolution is Critical**: The conflict resolution step is a vital security and data integrity feature. Without it, a malicious or simply outdated import file could silently overwrite a user's current, correct passwords. By pausing the import and forcing the user to make an explicit choice, the risk of unintentional data loss is significantly mitigated.
    *   **Re-authentication**: Requiring re-authentication before finalizing the import adds another layer of defense, ensuring that malware cannot programmatically trigger the final step of the import process without user consent.