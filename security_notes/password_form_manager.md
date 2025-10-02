# PasswordFormManager (`components/password_manager/core/browser/password_form_manager.cc`)

## 1. Summary

The `PasswordFormManager` is the stateful, central controller that orchestrates the entire lifecycle of a user's interaction with a credential form on a webpage. It acts as the "brain" of the password manager for a single form, from the moment the form is observed until a credential is saved, updated, or autofilled. It is responsible for parsing the form, fetching stored credentials, driving the autofill process, and managing the logic for provisional saving upon submission. Its complexity and central role make it one of the most security-critical components in the password management system.

## 2. Core Responsibilities & State Machine

A `PasswordFormManager` instance is created for each potential password form observed on a page. Its lifecycle is a complex state machine:

1.  **Observation & Initialization**:
    *   A `PasswordFormManager` is created with the initial `FormData` from the renderer.
    *   It immediately kicks off an asynchronous fetch for stored credentials via its `FormFetcher`. This brings back best matches, federated matches, and blocklist status from the `PasswordStore`.

2.  **Autofill**:
    *   Once the `FormFetcher` returns, `OnFetchCompleted` is called.
    *   The manager parses the observed form in `kFilling` mode using `FormDataParser`.
    *   It may **delay filling** to wait for server-side predictions (`FormPredictions`) to arrive, which can improve accuracy. This is managed by the `AsyncPredictionsWaiter`.
    *   Finally, `FillNow` calls `SendFillInformationToRenderer`, which provides the renderer with the credentials and instructions for filling the form.

3.  **Submission & Provisional Saving**:
    *   When the user submits the form, the `PasswordManager` calls `ProvisionallySave` on the corresponding `PasswordFormManager`.
    *   The manager re-parses the submitted form data, this time in `kSaving` mode. The results can differ from filling mode because the submitted values are now known.
    *   It creates a `pending_credentials` object, which represents the credential that might be saved.
    *   It uses a `PasswordSaveManager` to handle the UI (e.g., the save/update bubble) and the final commit to the `PasswordStore`.

## 3. Security-Critical Logic & Attack Surface

The `PasswordFormManager` is a convergence point for untrusted data from web pages, renderer processes, and potentially inaccurate server predictions. Its attack surface is vast.

*   **Username-First Flows (`HandleUsernameFirstFlow`)**: This is arguably the most complex and dangerous logic in the class. It attempts to link a username entered on one page to a password entered on a subsequent page.
    *   **Risk**: A logic flaw could cause the manager to incorrectly associate a username from `site-A.com` with a password prompt on `site-B.com`, leading to credentials being saved for the wrong site or a user being prompted to save a credential with the wrong username.
    *   **Mitigation**: The logic relies on a series of checks, including eTLD+1 matching (`IsPublicSuffixDomainMatch`), validating the freshness of the username data (`IsStale`), and a priority system (`UsernameFoundOutsideOfFormType`) to weigh the reliability of different signals (e.g., server predictions vs. local heuristics). A bug in any of these checks could break the security model.

*   **Form Parsing (`FormDataParser`)**: The security of both filling and saving depends entirely on correctly identifying the username, password, and new password fields.
    *   **Risk**: Mis-classification is a major risk. If a non-password field (e.g., a "PIN" or "Security Answer" field) is classified as a password, it could be saved as a password. If a password field is missed, a credential might not be saved at all.
    *   **Reliance on Predictions**: The parser uses both local heuristics and server-side `FormPredictions`. While predictions improve accuracy, they also introduce a dependency on an external service. A malicious or incorrect prediction could theoretically lead to incorrect parsing, although the code has checks to validate prediction structure.

*   **Provisional Saving (`ProvisionallySave`)**: This method takes the final `FormData` from the renderer and decides what to save.
    *   **Risk**: The manager must correctly determine which form fields correspond to the username and password in the final submitted data. An error could lead to saving the wrong values.
    *   **Username Updates (`OnUpdateUsernameFromPrompt`)**: When the user manually edits the username in the save bubble, the manager must correctly update its internal state. It tries to find the new username among other fields on the form (`all_alternative_usernames`) to improve crowdsourcing data, which is a complex and potentially fragile operation.

*   **Generated Passwords**: The manager handles presaving generated passwords before the form is even submitted.
    *   **Risk**: A logic error could cause a generated password to be associated with the wrong username or form, or fail to be saved entirely, forcing the user into a recovery flow.

## 4. Key Data Structures

*   **`observed_form_or_digest_`**: The canonical representation of the form being managed. This is either the `FormData` from the renderer or a `PasswordFormDigest` for non-HTML forms (like HTTP Basic Auth).
*   **`parsed_submitted_form_`**: The result of parsing the form *after* submission. This is the "ground truth" for what the user actually submitted and is the basis for what gets saved.
*   **`form_fetcher_`**: A helper class responsible for asynchronously fetching all relevant data from the `PasswordStore` for the current form.
*   **`password_save_manager_`**: A helper class that encapsulates the logic for the "save" and "update" UI and the final communication with the `PasswordStore`.
*   **`votes_uploader_`**: An optional helper that sends anonymized votes to Google's crowdsourcing backend to improve parsing heuristics.

## 5. Related Components

*   `components/password_manager/core/browser/form_data_parser.cc`: Performs the heavy lifting of parsing `FormData` into a structured `PasswordForm`.
*   `components/password_manager/core/browser/form_fetcher.cc`: Handles the asynchronous fetching of credentials from the `PasswordStore`.
*   `components/password_manager/core/browser/password_save_manager.cc`: Manages the UI and logic for saving or updating a credential after submission.
*   `components/password_manager/core/browser/field_info_manager.cc`: Provides the data (`PossibleUsernameData`) needed to implement username-first flows.