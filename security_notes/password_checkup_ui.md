# Password Checkup UI (`chrome/browser/extensions/api/passwords_private/password_check_delegate.cc`)

## 1. Summary

The `PasswordCheckDelegate` is the C++ backend and central controller for the Password Checkup feature within the `chrome://settings` page. It lives in the browser process and serves as the bridge between the high-level WebUI and the underlying password management services. It is responsible for handling all user-facing logic, such as starting a check, querying for insecure credentials, muting and unmuting alerts, and reporting the progress of a running check back to the UI. It translates the complex, asynchronous state of the backend services into a simplified model that the UI can easily consume.

## 2. Architecture: The `passwordsPrivate` API Backend

The `PasswordCheckDelegate` is a key part of the `passwordsPrivate` extension API, which is the mechanism used to expose browser-level password functionality to the semi-privileged settings WebUI.

1.  **Initialization**: The delegate is created along with the `PasswordsPrivateAPI` and immediately begins observing the core password services:
    *   `SavedPasswordsPresenter`: To get the list of all saved passwords.
    *   `InsecureCredentialsManager`: To get lists of credentials that have already been identified as weak, reused, or leaked.
    *   `BulkLeakCheckService`: To receive state updates about an ongoing bulk leak check.

2.  **Handling UI Requests**: The delegate's public methods correspond directly to actions a user can take on the settings page. When the JavaScript frontend calls `chrome.passwordsPrivate.startPasswordCheck()`, for example, the call is routed to `PasswordCheckDelegate::StartPasswordCheck()`.

3.  **Orchestration, Not Execution**: The delegate's primary role is orchestration. It does not perform the leak checks itself.
    *   To start a check, it uses a `BulkLeakCheckServiceAdapter` to call the `BulkLeakCheckService`.
    *   To get insecure credentials, it queries the `InsecureCredentialsManager`.
    *   It synthesizes the results from these different sources into a coherent state for the UI.

4.  **Communicating with the UI**:
    *   **Pull Model (`GetPasswordCheckStatus`)**: The UI can request the current status at any time. The delegate combines the state of the `BulkLeakCheckService` with its own internal progress tracking to provide a complete picture.
    *   **Push Model (`PasswordsPrivateEventRouter`)**: As the state changes (e.g., a credential check finishes, the entire check completes), the delegate uses the `PasswordsPrivateEventRouter` to proactively send events to the UI, causing it to update dynamically without needing to poll.

## 3. Security-Critical Logic

As the boundary between the browser process and the WebUI renderer, the `PasswordCheckDelegate` has several security-critical responsibilities.

*   **Data Sanitization for the UI**:
    *   **`ConstructInsecureCredentialUiEntry`**: This function is responsible for converting an internal `CredentialUIEntry` object (which contains the password) into a `PasswordUiEntry` object that is safe to send to the renderer. It critically **omits the password value** and other sensitive data, sending only the necessary information for display (username, origin, compromise type, etc.). This is a key defense against a compromised renderer process being able to exfiltrate passwords.

*   **Stable ID Management**:
    *   The delegate uses an `IdGenerator` to create stable, opaque integer IDs for each credential sent to the UI.
    *   When the UI sends a request back (e.g., "mute credential with id 123"), the delegate uses this ID to look up the original, trusted `CredentialUIEntry` object from its internal map. This prevents the UI from being able to craft a malicious request with arbitrary data; it can only act upon the credentials that the delegate has explicitly provided it an ID for.

*   **State Management and Progress Tracking**:
    *   The delegate uses a `PasswordCheckProgress` helper class to track the number of credentials being checked. This is more complex than a simple counter because the backend `BulkLeakCheckService` de-duplicates credentials before checking. The delegate correctly manages this by counting how many original passwords correspond to each unique canonical credential being checked.
    *   This ensures the progress bar shown to the user is accurate and prevents UI confusion. A bug in this counting logic could lead to a stalled or incorrect progress bar, potentially causing a user to cancel a check that was still running.

*   **Action Handling (`MuteInsecureCredential`)**: The delegate ensures that actions from the UI are applied to the correct credential by using the aforementioned ID-based lookup. This prevents any possibility of a "confused deputy" attack where the UI could trick the delegate into muting a different credential than the one the user selected.

## 4. The Frontend: `checkup_section.ts`

The `checkup_section.ts` file contains the TypeScript and Polymer-based implementation for the Password Checkup section of the `chrome://settings` page. It acts as the view layer, rendering the data provided by the C++ backend and forwarding user actions back to it.

*   **API Communication**: The component communicates exclusively through the `PasswordManagerImpl` singleton, which is a JavaScript wrapper around the `chrome.passwordsPrivate` extension API. This provides a clean, well-defined boundary between the frontend and the browser process.

*   **Data Flow (Backend -> Frontend)**: The component uses a combined pull-and-push model to stay in sync with the backend:
    1.  **Initial State**: In its `connectedCallback`, it makes one-time calls to `getPasswordCheckStatus()` and `getInsecureCredentials()` to fetch the initial state when the page is loaded.
    2.  **Live Updates**: It then registers listeners (`addPasswordCheckStatusListener`, `addInsecureCredentialsListener`) to receive asynchronous events from the `PasswordsPrivateEventRouter`. When the C++ backend has a new status or an updated list of insecure credentials, these listeners are invoked, updating the component's internal properties (`this.status_`, `this.compromisedPasswords_`, etc.) and causing the UI to re-render automatically via Polymer's data-binding system.

*   **Action Flow (Frontend -> Backend)**: User actions are handled by simple, direct calls to the API wrapper:
    *   **Start Check**: The "Check passwords" button's `on-click` handler calls `onPasswordCheckButtonClick_`, which in turn calls `PasswordManagerImpl.getInstance().startBulkPasswordCheck()`.
    *   **Automatic Start**: The component observes the URL for a `?start_check=true` parameter. If found, it automatically calls `startBulkPasswordCheck()`, allowing other parts of Chrome (like Safety Hub) to link directly to the checkup and initiate a scan.
    *   **Navigation**: Clicking on a result row (e.g., "Compromised passwords") uses a client-side `Router` to navigate to the appropriate details subpage.

*   **Security Posture**: The frontend's security posture is strong due to its simplicity and clear separation of concerns.
    *   **No Sensitive Data**: The frontend **never handles plaintext passwords**. All data it receives via the `passwordsPrivate` API in the `PasswordUiEntry` objects has already been sanitized by the `PasswordCheckDelegate`.
    *   **No Business Logic**: The frontend contains no complex business logic. It simply renders the state provided by the backend and forwards user intents. All security-critical decisions (how to check, what to check, how to store results) are handled entirely within the C++ backend. This makes the frontend a "dumb" view, which is a robust security design.

## 5. Related Components

*   `chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h`: The class responsible for sending asynchronous events (like status updates) from the C++ backend to the JavaScript frontend.
*   `components/password_manager/core/browser/ui/bulk_leak_check_service_adapter.h`: The adapter used to interface with the `BulkLeakCheckService`.
*   `components/password_manager/core/browser/ui/insecure_credentials_manager.h`: The service that provides the lists of weak and reused passwords.