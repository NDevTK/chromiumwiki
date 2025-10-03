# Security Notes: `content/browser/loader/file_url_loader_factory.cc`

## File Overview

This file implements the `FileURLLoaderFactory`, a critical security boundary in the browser. It is responsible for handling all network requests for the `file://` scheme. Since these URLs map directly to the user's local filesystem, this factory is the gatekeeper that prevents web content and other less-privileged processes from arbitrarily reading local files. Its security is paramount to containing the web within its sandbox and protecting user data.

## Key Security Mechanisms and Patterns

### 1. The Central Access Control Check: `IsFileAccessAllowed`

The single most important security decision in this file is delegated to the `ContentBrowserClient`. This design allows the embedder (e.g., Chrome) to define its own specific file access policies.

-   **Mechanism**: Before a file is opened, the code calls `GetContentClient()->browser()->IsFileAccessAllowed(path, full_path, profile_path)`.
-   **Security Implication**: This check is the primary defense. It is responsible for enforcing rules such as:
    -   Preventing web pages from making `file://` requests.
    -   Allowing `file://` requests only when the `--allow-file-access-from-files` switch is present.
    -   Granting specific extensions or components the privilege to access local files.
-   A failure to call this function, or a bug in the embedder's implementation of it, would be a critical vulnerability leading to universal file read access.

### 2. Explicit Security Policies (`FileAccessPolicy`)

The factory's behavior is controlled by an explicit `FileAccessPolicy` enum, which makes the security context clear.

-   **`kRestricted`**: This is the default and secure mode. It ensures that the `IsFileAccessAllowed` check described above is always performed.
-   **`kUnrestricted`**: This mode **bypasses** the `IsFileAccessAllowed` check entirely. It is used by the function `CreateFileURLLoaderBypassingSecurityChecks`.
-   **Security Implication**: The existence of an "unrestricted" mode is a significant finding. This function is intended for trusted, internal browser components that need to read local files (e.g., for loading configuration or displaying local help pages). Any way for a renderer or untrusted process to request a loader factory with this policy would be a critical sandbox escape.

### 3. Same-Origin Policy and CORS for `file://`

The factory correctly handles the complex security policies surrounding `file://` origins.

-   **Opaque Origins**: `file://` URLs have opaque origins, meaning they are not same-origin with each other unless explicitly allowed.
-   **`kDisableWebSecurity`**: The code correctly honors the `--disable-web-security` switch, which is a global kill-switch for all origin checks.
-   **`shared_cors_origin_access_list_`**: For requests that are not same-origin, the factory consults this access list. This is the mechanism that allows specific origins (like a privileged extension's origin) to be granted access to `file://` URLs, as an exception to the normal same-origin policy.

### 4. Safe Handling of Directory Listings and Links

The factory has logic to handle directory URLs and Windows shortcuts (`.lnk` files), both of which have security implications.

-   **Directory Listings**: If a requested URL points to a directory, the factory does not serve the file list directly. Instead, it issues a `301 Moved Permanently` redirect to a URL with a trailing slash. This triggers a new request that is then handled by the dedicated `FileURLDirectoryLoader` class. This clean separation ensures that directory listing logic is isolated. The initial `IsFileAccessAllowed` check still applies, preventing unauthorized directory listings.
-   **Link Following**: On Windows, if a request is for a `.lnk` file, the factory resolves the shortcut's target path and issues a `301` redirect to the new `file://` URL.
-   **Security Implication for Redirects**: The use of redirects is a secure pattern here. It forces the entire loading mechanism to re-evaluate the new, resolved URL from the beginning. This ensures that all security checks, including `IsFileAccessAllowed`, are performed on the *actual target path*, not just the initial link path, preventing a class of "time-of-check to time-of-use" (TOCTOU) vulnerabilities.

## Summary of Security Posture

The `FileURLLoaderFactory` is a well-hardened security boundary.

-   **Security Model**: It operates on a principle of least privilege, denying file access by default and delegating the decision to grant access to the trusted `ContentBrowserClient`.
-   **Clear Security Policies**: The use of explicit enums like `FileAccessPolicy` and `DirectoryLoadingPolicy` makes the intended security posture of each loader clear and auditable.
-   **Robust Handling of Edge Cases**: The redirect-based approach for handling directories and links is a strong pattern that prevents TOCTOU bugs.
-   **Primary Risk**: The main risk associated with this component is the existence of the `CreateFileURLLoaderBypassingSecurityChecks` function. While intended for trusted callers, any vulnerability that allows an untrusted process to invoke this function would lead to a complete bypass of the file access security model. Therefore, auditing the call sites of this function is a high-priority task for a security researcher.