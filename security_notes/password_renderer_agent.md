# Password Renderer Agent (`components/autofill/content/renderer/password_autofill_agent.cc`)

## 1. Summary

The `PasswordAutofillAgent` is the renderer-side counterpart to the browser-process password management system. It lives within the sandboxed renderer process for each frame and is responsible for all direct interaction with the DOM. Its core duties are to receive autofill data from the browser, securely fill it into web forms, and report user interactions and form submissions back to the browser. As the component that bridges the trusted browser process and the untrusted web content, its design is critical to the security of the entire password autofill feature.

## 2. Architecture: A Mojo-Driven DOM Manipulator

The agent's architecture is that of a trusted executor operating in an untrusted environment. It receives commands from the browser via a Mojo interface and executes them with minimal local decision-making.

1.  **Browser-to-Renderer Communication**: The agent implements the `mojom::PasswordAutofillAgent` interface. Its primary entry point is `ApplyFillDataOnParsingCompletion`, which is called by the browser process after it has parsed a form and fetched relevant credentials. This method receives a `PasswordFormFillData` object containing the username and password values to be filled, along with the specific `FieldRendererId`s of the target elements.

2.  **Renderer-to-Browser Communication**: The agent holds a remote to a `mojom::PasswordManagerDriver` interface, which it uses to send information back to the browser process. Key events it reports include:
    *   `PasswordFormsParsed` and `PasswordFormsRendered`: Sent when the agent scans the DOM, providing the browser with the structure of potential credential forms.
    *   `PasswordFormSubmitted`: Sent after the agent detects a form submission. It sends the full `FormData` back to the browser for analysis.
    *   `InformAboutUserInput`: Notifies the browser whenever the user modifies a field in a password form.

3.  **DOM Interaction**: The agent uses utility functions from `components/autofill/content/renderer/form_autofill_util.h` to interact with the DOM. All element identification is based on the stable `FieldRendererId` provided by the browser, not on potentially unreliable information like element names or IDs from the DOM itself.

4.  **Prerendering Handling**: The agent has a special `DeferringPasswordManagerDriver` that it uses when the page is being prerendered. This class intercepts all outgoing Mojo calls and queues them to be executed only after the page is actually activated. This is a crucial mechanism to prevent a background prerendering page from interacting with the password manager UI or state.

## 3. Security-Critical Logic & Attack Surface

The agent's primary security responsibility is to prevent a malicious webpage from stealing a user's password as it is being filled.

*   **Secure Filling (`PasswordValueGatekeeper`)**: This is the single most important security mechanism in the class. When autofilling a password, the agent does not immediately set the `.value` of the password field.
    1.  Instead, `FillFieldAutomatically` calls `SetSuggestedValue`, which places the password in an internal Blink state, and then registers the element with the `PasswordValueGatekeeper`.
    2.  The gatekeeper holds a list of these "pending" password fields.
    3.  Only after a `UserGestureObserved()` event (e.g., a click or keypress) is received does the gatekeeper's `OnUserGesture()` method execute.
    4.  This method iterates through the pending fields and calls `SetAutofillValue`, which finally transfers the password from the internal suggested state into the element's actual value, making it visible to the DOM and accessible to scripts.
    *   **Security Guarantee**: This two-phase process ensures that a page cannot use JavaScript to programmatically trigger an autofill and immediately read the filled password. The fill is only completed after the browser has observed a legitimate user interaction with the page, defeating scripted attacks.

*   **Frame Security Checks**:
    *   `FrameCanAccessPasswordManager()`: Before performing any actions, the agent verifies that the frame's origin is allowed to use the password manager. It explicitly blocks non-HTTP/HTTPS schemes (e.g., `about:`, `data:`) and respects Blink's `CanAccessPasswordManager` security policy.
    *   `IsInCrossOriginIframeOrEmbeddedFrame()`: When filling credentials, the agent checks if the target element is in a cross-origin iframe relative to its parent frames. This check helps mitigate attacks where a malicious ad or embedded content could try to create a password field to steal credentials intended for the top-level site.

*   **Renderer as a "Dumb" Executor**: The agent makes very few security decisions on its own. It receives instructions from the browser (e.g., "fill this password into the field with this ID") and executes them. When a form is submitted, it does not decide what to save; it sends the *entire form's data* back to the browser process (`PasswordFormManager`), which then re-validates everything in a privileged context before making a decision. This minimizes the trust placed in the sandboxed renderer process.

## 4. Related Components

*   `mojom::PasswordAutofillAgent` & `mojom::PasswordManagerDriver`: The Mojo interfaces that define the security boundary and communication channel between the renderer and the browser.
*   `components/autofill/content/renderer/form_autofill_util.h`: The library of functions for safely finding and manipulating form elements in the DOM based on renderer IDs.
*   `components/password_manager/core/browser/password_manager.cc`: The browser-side component that ultimately drives the agent via the Mojo interface.
*   `PasswordGenerationAgent`: The renderer-side agent responsible for handling the password generation flow, which works in close concert with the `PasswordAutofillAgent`.