# Security Analysis of extensions/browser/user_script_manager.cc

## 1. Overview

The `UserScriptManager` is a browser-side component that orchestrates the loading and management of all content scripts. This includes both static content scripts defined in an extension's `manifest.json` and dynamic scripts added at runtime via the `userScripts` API (a more powerful and now-deprecated predecessor to the `scripting` API) or by an embedder (like a `<webview>`).

This manager doesn't load scripts itself. Instead, it acts as a factory and supervisor for `UserScriptLoader` objects. It creates and maintains a separate `UserScriptLoader` for each extension and for each embedder context that uses scripts. Its primary responsibility is to ensure that the correct loaders are active for loaded extensions and that they are destroyed when extensions are unloaded.

This component is a critical security boundary because it is the first point of contact in the browser process for initiating the content script injection mechanism. Its correct operation is fundamental to ensuring that only scripts from enabled, valid extensions are prepared for injection.

## 2. Core Security Concepts & Mechanisms

### 2.1. Loader Scoping and Lifecycle Management

The most important function of the `UserScriptManager` is to maintain a strict one-to-one mapping between an extension (or embedder) and its `UserScriptLoader`.

-   **Data Structures**: The manager uses two maps:
    -   `extension_script_loaders_`: Maps an `ExtensionId` to its dedicated `ExtensionUserScriptLoader`.
    -   `embedder_script_loaders_`: Maps a `mojom::HostID` (which can represent a WebUI or other embedder) to its `EmbedderUserScriptLoader`.
-   **Lazy Instantiation**: Loaders are created on-demand. When a script needs to be loaded for an extension for the first time (typically in `OnExtensionLoaded`), `GetUserScriptLoaderForExtension` is called, which then calls `CreateExtensionUserScriptLoader` to instantiate and store the loader.
-   **`ExtensionRegistryObserver`**: The `UserScriptManager` observes the `ExtensionRegistry`. This is the core of its security model for lifecycle management.
    -   **`OnExtensionLoaded`**: When an extension is loaded, the manager ensures a loader exists and tells it to begin loading its scripts (`loader->AddScriptsForExtensionLoad`).
    -   **`OnExtensionUnloaded`**: When an extension is unloaded, the corresponding loader is **completely erased** from the `extension_script_loaders_` map. This is a critical cleanup step. It ensures that a disabled or uninstalled extension has no active loader in the browser, and therefore no ability to inject scripts.

**Security Criticality**: The strict lifecycle management tied to the `ExtensionRegistry` is paramount. If a loader were not properly destroyed upon extension unload, it could potentially lead to a "zombie" script from a disabled extension being injected into a page. The current implementation correctly handles this by removing the loader from its map.

### 2.2. UserScripts API Permission and Toggling

The manager also handles the logic for the powerful (and now developer-mode-only) `userScripts` API. This API allows extensions to register content scripts dynamically.

-   **`AreUserScriptsAllowed`**: This is the central permission check for the `userScripts` API. It verifies two things:
    1.  That the extension has the `userScripts` permission in its manifest.
    2.  That a per-extension preference (`kUserScriptsAllowedPref`) is enabled.
-   **Migration Logic**: The manager contains logic (`MigrateUserScriptExtensions`) to handle a one-time migration from an old model where this was controlled by a global "developer mode" toggle to the current per-extension toggle.
-   **Enabling/Disabling in Loader**: When the user toggles the preference, `SetUserScriptPrefEnabled` is called. This not only updates the preference but also directly calls `loader->SetSourceEnabled(UserScript::Source::kDynamicUserScript, enabled)`. This tells the active loader to either enable or disable its dynamically registered scripts.

**Security Criticality**: The `userScripts` API is highly privileged because it allows for dynamic, persistent script injection. Gating this behind a specific manifest permission and a user-controlled toggle is the primary security mitigation. The `UserScriptManager` is responsible for correctly checking this state (`AreUserScriptsAllowed`) and propagating it to the loader. A bug here could allow an extension to use this API without proper consent.

### 2.3. Initial Load Coordination

The manager coordinates the initial loading of all content scripts at browser startup.

-   **`pending_initial_extension_loads_`**: This set tracks the extensions whose scripts are currently being loaded from disk by their respective loaders.
-   **`OnInitialExtensionLoadComplete`**: When a loader finishes, it calls this callback. The manager removes the extension from the pending set.
-   **`SignalContentScriptsLoaded`**: Once the pending set is empty, the manager calls `ExtensionsBrowserClient::Get()->SignalContentScriptsLoaded()`. This is a system-wide signal that content scripts are ready, which other parts of the browser (like the `UserScriptListener`) can use to know it's safe to proceed with operations that depend on scripts being loaded.

**Security Criticality**: This coordination prevents race conditions. For example, a new tab might be opened before all content scripts have been loaded from disk. By providing a definitive "all clear" signal, the `UserScriptManager` ensures that the system doesn't try to inject scripts before they are ready, which could lead to pages being rendered without their intended scripts.

## 4. Potential Attack Vectors & Security Risks

1.  **Loader Lifecycle Mismatch**: The most significant risk would be a bug in the `OnExtensionUnloaded` handler. If it failed to erase a loader from its map, the loader could persist and potentially inject scripts after its corresponding extension has been disabled, leading to a zombie script scenario.
2.  **Permission Check Bypass**: A logic flaw in `AreUserScriptsAllowed` that incorrectly evaluates the user's preference or the manifest permission could allow an extension to use the privileged `userScripts` API when it shouldn't.
3.  **State Corruption**: As the manager holds the authoritative maps of loaders, a memory corruption vulnerability that allowed an attacker to tamper with these maps could be severe. For example, an attacker could swap the loader for a benign extension with one for a malicious extension.

## 5. Conclusion

The `UserScriptManager` serves as a secure and robust "factory" and supervisor for `UserScriptLoader` instances. Its security model is not based on making fine-grained decisions about individual script injections, but rather on the coarse-grained, critical task of ensuring that:
1.  Only loaded and enabled extensions have an active `UserScriptLoader`.
2.  Loaders are promptly and completely destroyed when an extension is unloaded.
3.  Privileged features like the `userScripts` API are gated behind the correct permission and preference checks.

It correctly delegates the actual file I/O and parsing to the individual loaders, adhering to a clean separation of concerns. Its role as the lifecycle and policy manager for content script loaders is a cornerstone of the extension system's security.