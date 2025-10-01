# PasswordForm (`components/password_manager/core/browser/password_form.h`)

## 1. Summary

`PasswordForm` is one of the most security-critical data structures in the entire Chromium codebase. It is the canonical representation of a user's credential, encapsulating not just the username and password, but a vast amount of metadata related to the credential's origin, usage, and security status.

This struct is the fundamental unit of data that is passed between the password store (database), the form parsing logic, the autofill controller, and the password manager UI. Its integrity and the correctness of the code that handles it are paramount to protecting user credentials.

## 2. Core Concepts

`PasswordForm` serves multiple purposes, acting as a container for:

*   **Observed Forms:** When a user interacts with a form on a webpage, a `PasswordForm` is created to represent the structure and content of that form.
*   **Saved Credentials:** When a credential is saved to the `PasswordStore`, it is stored as a `PasswordForm` record.
*   **Security State:** The struct is the central location for storing information about the security health of a credential, powering features like Password Checkup.

## 3. Security-Critical Data Members

Nearly every field has security implications, but some are particularly critical:

*   **`signon_realm`**: The effective "security origin" of the credential (e.g., `https://example.com:443`). This is the primary key for matching credentials and is fundamental to preventing passwords from being filled into the wrong site.

*   **`url` and `action`**: These provide more specific details about the page and form where the credential was used. An incorrect `action` URL could cause autofill to fail or, in a worst-case scenario, fill a password into a form that submits to a malicious endpoint.

*   **`username_value` and `password_value`**: The plaintext credentials. These are the crown jewels and must be handled with extreme care, ensuring they are only exposed when explicitly requested by the user (e.g., for filling) and are properly protected at rest.

*   **`password_issues`**: A `flat_map` that is the data model for the **Password Checkup** feature.
    *   **Key:** `InsecureType` (enum: `kLeaked`, `kPhished`, `kWeak`, `kReused`).
    *   **Value:** `InsecurityMetadata`, a struct containing:
        *   `is_muted`: A boolean indicating if the user has dismissed the warning for this specific issue.
        *   `trigger_notification_from_backend`: A flag to coordinate notifications between the client and Google's backend.
    *   This map directly tracks known vulnerabilities associated with the credential, making it highly sensitive data.

*   **`in_store`**: An enum (`Store`) indicating where the credential lives:
    *   `kProfileStore`: Stored only locally on the user's device, protected by the OS's encryption mechanisms (e.g., Keychain on macOS, DPAPI on Windows).
    *   `kAccountStore`: Synced with the user's Google Account and stored on Google's servers.
    *   The distinction is critical for privacy and data locality. A bug that causes a credential to be saved to the wrong store would be a severe privacy violation.

## 4. Security Considerations & Attack Surface

*   **Credential Leaks:** The most obvious risk is a bug that causes a `PasswordForm` object to be logged, serialized to an insecure location, or sent over the network unencrypted.
*   **Cross-Site Filling/Autofill:** The entire security of password autofill relies on correctly matching a saved `PasswordForm` to a form on a page. A flaw in the matching logic (which relies on `signon_realm`, `url`, `action`, etc.) could lead to a user's password for `bank.com` being filled into a form on a phishing site `bank.com.attacker.com`.
*   **Information Disclosure via Metadata:** Even without the password itself, the metadata is sensitive. For example, the `password_issues` map reveals known vulnerabilities in a user's accounts. The `times_used_in_html_form` and `date_last_used` fields provide a history of user activity.
*   **Federated Credentials (`federation_origin`):** These credentials (e.g., "Sign in with Google") have a different trust model. A bug that confuses a federated credential with a regular password credential could break logins or create security holes.

## 5. Related Files

*   `components/password_manager/core/browser/password_store/`: The backend interface for persisting and retrieving `PasswordForm` objects from the database.
*   `components/password_manager/core/browser/password_form_manager.cc`: The "controller" class that orchestrates the lifecycle of a `PasswordForm`, from parsing it on a page to saving or updating it in the store.
*   `components/password_manager/core/browser/password_autofill_manager.cc`: The component responsible for taking `PasswordForm` data and actually filling it into web pages.
*   `components/password_manager/core/browser/leak_detection/`: The feature that populates the `kLeaked` entry in the `password_issues` map.