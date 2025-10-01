# URLSecurityManager (`net/http/url_security_manager.h`)

## 1. Summary

The `URLSecurityManager` is a critical component for managing security policies related to network authentication in Chromium. Its primary responsibility is to control when it is safe to automatically send a user's default credentials (e.g., for NTLM or Kerberos authentication) to a server or to delegate user credentials to a server.

This acts as a gatekeeper to prevent accidental leakage of sensitive credentials to untrusted or malicious endpoints.

## 2. Core Concepts

The manager uses two distinct allow lists to enforce its policies:

*   **Default Credentials Allowlist (`allowlist_default_`):**
    *   **Purpose:** Governs which servers are trusted to receive the user's default credentials for authentication.
    *   **Platform Behavior:**
        *   **Windows:** If this list is *empty*, the manager falls back to using the Windows **Security Zone mapping**. Typically, servers in the "Local Intranet" zone are allowed to receive credentials. This offloads the trust decision to the underlying OS configuration.
        *   **Other Platforms (macOS, Linux, etc.):** If this list is *empty*, no servers are trusted by default. All servers must be explicitly added to the allowlist.

*   **Delegation Allowlist (`allowlist_delegate_`):**
    *   **Purpose:** Governs which servers are allowed to receive delegated Kerberos tickets. This allows a service to impersonate the user and access other network resources on their behalf.
    *   **Behavior:** This feature is more restrictive. If the list is empty, delegation is disallowed for all servers, regardless of the platform.

## 3. Key Classes and Methods

*   **`URLSecurityManager` (Abstract Base Class):**
    *   `Create()`: A static factory method that instantiates the appropriate platform-specific implementation (e.g., `URLSecurityManagerWin` or `URLSecurityManagerPosix`).
    *   `CanUseDefaultCredentials(const url::SchemeHostPort&)`: The core method to check if default credentials can be sent to a specific server.
    *   `CanDelegate(const url::SchemeHostPort&)`: Checks if credential delegation is permitted for a specific server.

*   **`URLSecurityManagerAllowlist` (Default/Posix Implementation):**
    *   Provides a concrete implementation that manages the two allowlists using `HttpAuthFilter` objects.

## 4. Security Considerations & Attack Surface

*   **Credential Leakage:** The primary risk is the leakage of NTLM or Kerberos credentials to an attacker-controlled server. If a malicious server can get itself into the "Local Intranet" zone on a Windows machine (e.g., via DNS spoofing or other network attacks), it could trigger automatic credential sending.
*   **Misconfiguration:** Enterprise policies that incorrectly configure the allowlists or Windows Security Zones could inadvertently expose credentials.
*   **Kerberos Delegation:** Delegation is a powerful feature. If a server in the delegate allowlist is compromised, an attacker could potentially impersonate users and move laterally across the network.
*   **Zone-Mapping Flaws:** Any vulnerability in how Windows determines security zones could be leveraged to bypass the protections offered by this manager. It's an important piece of the underlying trust model on Windows.

## 5. Related Files

*   `net/http/url_security_manager_win.cc`: Windows-specific implementation that uses the `IInternetSecurityManager` COM interface to check security zones.
*   `net/http/url_security_manager_posix.cc`: A simpler implementation for non-Windows platforms that relies solely on the configured allowlists.
*   `net/http/http_auth_filter.h`: Defines the filter used to represent the allowlists.