# WebUI Security Model

WebUI pages, those served from the `chrome://` scheme, are a fundamental part of the Chromium browser's user interface for features like Settings, History, and Bookmarks. These pages require elevated privileges to interact with internal browser systems. This document outlines the architecture and security model that allows them to do so safely.

## WebUI Architecture

The WebUI system is built on a C++ backend that lives in the browser process and a standard HTML/CSS/JavaScript frontend that runs in a sandboxed renderer process.

1.  **`WebUIControllerFactory`**: When a navigation to a `chrome://` URL occurs, the `WebUIControllerFactoryRegistry` is consulted. It finds the specific factory responsible for that URL (e.g., `chrome://settings`).

2.  **`WebUIController`**: The factory creates a `WebUIController` subclass (e.g., `SettingsUI`). This C++ object is the "backend" for the page, responsible for its logic and data handling.

3.  **`WebUIImpl`**: The `WebUIController` is owned by a `WebUIImpl` object. This class acts as the bridge between the browser and the renderer. Its two most critical functions are:
    *   Serving the page's resources (HTML, JS, CSS) from the internal resource bundle, not from the network. This prevents external code injection.
    *   Establishing a dedicated Mojo IPC pipe between the browser process and the WebUI's renderer process.

## The Privilege Granting Process

A renderer process is not inherently privileged. It is granted special capabilities only when it is designated to host a WebUI page. This process is tightly controlled and happens within `RenderFrameHostImpl`.

1.  **Navigation and Controller Creation**: When a `chrome://` navigation is committed for a specific `RenderFrameHost`, a `WebUIImpl` and its corresponding `WebUIController` are created and associated with that frame.

2.  **`RenderFrameHostImpl::SetWebUI`**: This method is called, which begins the privilege escalation process for the renderer.

3.  **`RenderFrameHostImpl::AllowBindings`**: `SetWebUI` calls `AllowBindings`, passing in the specific set of privileges the page needs (e.g., `BindingsPolicy::kWebUi`). This method is the primary security gate and performs two critical checks:
    *   **Process Purity**: It verifies that the renderer process has not been "contaminated" by previously hosting any non-WebUI content. It iterates through all frames in the process and ensures none have committed a real navigation. This is a critical defense-in-depth measure to prevent a process that may have been exposed to web content from gaining powerful browser bindings.
    *   **Process Lock**: It asserts that the renderer process is locked to the specific `chrome://` site URL. This leverages the Site Isolation architecture to guarantee that the process is dedicated to this WebUI origin and cannot be used to host other sites.

4.  **`ChildProcessSecurityPolicy::GrantWebUIBindings`**: Only after these checks pass does `AllowBindings` call `GrantWebUIBindings`. This is the authoritative step where the central `ChildProcessSecurityPolicy` singleton records that the renderer process ID is officially permitted to use WebUI bindings.

Once the bindings are granted, the Mojo pipe set up by `WebUIImpl` becomes a trusted channel. The WebUI page can use this pipe to call Mojo APIs exposed by the browser process that are completely inaccessible to normal web pages.

## Untrusted Content: `chrome-untrusted://`

To safely display potentially untrustworthy content (e.g., an image from the web) within a privileged UI, Chromium uses the `chrome-untrusted://` scheme.

*   These pages are backed by an `UntrustedWebUIController`.
*   The key security feature is that `UntrustedWebUIController` **explicitly disables all WebUI bindings**.
*   This creates a strong security boundary, allowing the `chrome-untrusted://` page to be embedded in a privileged `chrome://` page (e.g., in an `<iframe>`) without any risk of the untrusted content gaining access to the privileged Mojo APIs. It has a unique, isolated origin and cannot script its embedder.