# ChildProcessSecurityPolicyImpl

This page details the `ChildProcessSecurityPolicyImpl` class and its role in site isolation.

## Core Concepts

The `ChildProcessSecurityPolicyImpl` class is a central component in Chromium's security architecture. It is responsible for managing the security policy of child processes, primarily renderer processes, ensuring they operate within a secure sandbox and adhere to the principle of least privilege. 

`ChildProcessSecurityPolicyImpl` acts as an authority for granting and checking permissions for various resources, including:

*   **File access:** Controlling read, write, create, and delete operations on files.
*   **File system access:** Managing access to sandboxed file systems.
*   **URL access:** Regulating the ability of renderer processes to request and commit URLs, including specific schemes and origins.
*   **WebUI bindings:** Managing the granting of WebUI capabilities to renderer processes.
*   **Raw cookies:** Controlling access to read raw cookies.
*   **MIDI:** Managing permissions for sending MIDI and MIDI SysEx messages.

`ChildProcessSecurityPolicyImpl` plays a crucial role in enforcing site isolation by working in conjunction with `ProcessLock`. It uses process locks to restrict renderer processes to specific sites or origins, preventing them from accessing resources from unauthorized sites.

The class enforces security policies through various `Can*` methods, such as `CanCommitURL`, `CanReadFile`, and `CanRequestURL`. These methods are used to check if a child process has the necessary permissions to perform a specific action before allowing it to proceed.

The `SecurityState` class, nested within `ChildProcessSecurityPolicyImpl`, is used to store the security state information for each child process. This state includes details about granted permissions, process lock information, and browsing instance data.

## Key Areas of Concern

The `ChildProcessSecurityPolicyImpl` class is critical for maintaining Chromium's security posture. Incorrectly managing permissions within this class can lead to significant security vulnerabilities. Key areas of concern include:

-   **Incorrect Permission Granting:** Accidentally granting excessive permissions to a child process can расширить its capabilities beyond what is necessary, potentially allowing it to bypass security restrictions and access sensitive resources or data from other sites. This could lead to vulnerabilities such as cross-site scripting (XSS) or unauthorized data exfiltration.
-   ** flawed Permission Checks:** Errors or oversights in the permission checking logic within the `Can*` methods can result in unauthorized access to resources. If these checks fail to adequately validate the security context or resource access requests, malicious actors could potentially exploit these flaws to gain unauthorized access and compromise the security of the browser or user data.
-   **Interaction with Site Isolation Mechanisms:** The security policy enforced by `ChildProcessSecurityPolicyImpl` is tightly coupled with other site isolation mechanisms, such as process locks and site isolation policies. Issues in the interaction between these components can lead to unexpected security gaps or bypasses. For example, inconsistencies in how process locks are applied or enforced in conjunction with the security policy could undermine the effectiveness of site isolation and create opportunities for cross-site data breaches.

### Related Files

-   `content/browser/child_process_security_policy_impl.cc`
-   `content/browser/child_process_security_policy_impl.h`

### Functions and Methods

-   `ChildProcessSecurityPolicyImpl::Add`: Adds a new child process to the security policy.
-   `ChildProcessSecurityPolicyImpl::Remove`: Removes a child process from the security policy.
-   `ChildProcessSecurityPolicyImpl::GrantCommitURL`: Grants permission to commit a URL.
    -   This method also handles special cases for blob and filesystem URLs.
-   `ChildProcessSecurityPolicyImpl::GrantRequestOfSpecificFile`: Grants permission to request a specific file.
-   `ChildProcessSecurityPolicyImpl::GrantReadFile`: Grants permission to read a file.
-   `ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFile`: Grants permission to create, read, and write a file.
-   `ChildProcessSecurityPolicyImpl::GrantCopyInto`: Grants permission to copy into a directory.
-   `ChildProcessSecurityPolicyImpl::GrantDeleteFrom`: Grants permission to delete from a directory.
-   `ChildProcessSecurityPolicyImpl::GrantPermissionsForFile`: Grants certain permissions to a file.
-   `ChildProcessSecurityPolicyImpl::RevokeAllPermissionsForFile`: Revokes all permissions granted to a file.
-   `ChildProcessSecurityPolicyImpl::GrantReadFileSystem`: Grants permission to read a file system.
-   `ChildProcessSecurityPolicyImpl::GrantWriteFileSystem`: Grants permission to write to a file system.
-   `ChildProcessSecurityPolicyImpl::GrantCreateFileForFileSystem`: Grants permission to create a file in a file system.
-   `ChildProcessSecurityPolicyImpl::GrantCreateReadWriteFileSystem`: Grants permission to create, read, and write to a file system.
-   `ChildProcessSecurityPolicyImpl::GrantCopyIntoFileSystem`: Grants permission to copy into a file system.
-   `ChildProcessSecurityPolicyImpl::GrantDeleteFromFileSystem`: Grants permission to delete from a file system.
-   `ChildProcessSecurityPolicyImpl::GrantSendMidiMessage`: Grants permission to send MIDI messages.
-   `ChildProcessSecurityPolicyImpl::GrantSendMidiSysExMessage`: Grants permission to send MIDI SysEx messages.
-   `ChildProcessSecurityPolicyImpl::GrantCommitOrigin`: Grants permission to commit a URL with a specific origin.
-   `ChildProcessSecurityPolicyImpl::GrantRequestOrigin`: Grants permission to request a URL with a specific origin.
-   `ChildProcessSecurityPolicyImpl::GrantRequestScheme`: Grants permission to request a URL with a specific scheme.
-   `ChildProcessSecurityPolicyImpl::GrantWebUIBindings`: Grants WebUI bindings to a child process.
-   `ChildProcessSecurityPolicyImpl::GrantReadRawCookies`: Grants permission to read raw cookies.
-   `ChildProcessSecurityPolicyImpl::RevokeReadRawCookies`: Revokes permission to read raw cookies.
-   `ChildProcessSecurityPolicyImpl::GrantOriginCheckExemptionForWebView`: Grants an origin check exemption for a WebView.
-   `ChildProcessSecurityPolicyImpl::HasOriginCheckExemptionForWebView`: Checks if an origin has an exemption for a WebView.
-   `ChildProcessSecurityPolicyImpl::CanCommitURL`: Checks if a child process is allowed to commit a URL.
    -   This method also handles special cases for blob and filesystem URLs.
    -   It also checks if the URL is a pseudo scheme.
-   `ChildProcessSecurityPolicyImpl::CanRequestURL`: Checks if a child process is allowed to request a URL.
    -   This method also handles special cases for file and content URLs.
-   `ChildProcessSecurityPolicyImpl::HasPermissionsForFile`: Checks if a child process has certain permissions for a file.
-   `ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystemFile`: Checks if a child process has certain permissions for a file system file.
-   `ChildProcessSecurityPolicyImpl::CanReadFileSystemFile`: Checks if a child process is allowed to read a file system file.
-   `ChildProcessSecurityPolicyImpl::CanWriteFileSystemFile`: Checks if a child process is allowed to write to a file system file.
-   `ChildProcessSecurityPolicyImpl::CanCreateFileSystemFile`: Checks if a child process is allowed to create a file in a file system.
-   `ChildProcessSecurityPolicyImpl::CanCreateReadWriteFileSystemFile`: Checks if a child process is allowed to create, read, and write to a file system file.
-   `ChildProcessSecurityPolicyImpl::CanCopyIntoFileSystemFile`: Checks if a child process is allowed to copy into a file system file.
-   `ChildProcessSecurityPolicyImpl::CanDeleteFromFileSystemFile`: Checks if a child process is allowed to delete from a file system file.
-   `ChildProcessSecurityPolicyImpl::CanMoveFileSystemFile`: Checks if a child process is allowed to move a file system file.
-   `ChildProcessSecurityPolicyImpl::CanCopyFileSystemFile`: Checks if a child process is allowed to copy a file system file.
-   `ChildProcessSecurityPolicyImpl::HasWebUIBindings`: Checks if a child process has WebUI bindings.
-   `ChildProcessSecurityPolicyImpl::CanReadRawCookies`: Checks if a child process is allowed to read raw cookies.
-   `ChildProcessSecurityPolicyImpl::CanSendMidiMessage`: Checks if a child process is allowed to send MIDI messages.
-   `ChildProcessSecurityPolicyImpl::CanSendMidiSysExMessage`: Checks if a child process is allowed to send MIDI SysEx messages.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `AddCommittedOrigin` method to add a committed origin to a child process.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `AddOriginIsolationStateForBrowsingInstance` method to add an origin isolation state for a browsing instance.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `AddFutureIsolatedOrigins` method to add future isolated origins.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `AddIsolatedOriginInternal` method to add an isolated origin.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `RemoveOptInIsolatedOriginsForBrowsingInstanceInternal` method to remove opt-in isolated origins for a browsing instance.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `ClearBrowserContextIfMatches` method to clear the browser context if it matches a given browser context.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `GetSecurityState` method to get the security state for a child process.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `GetKilledProcessOriginLock` method to get the killed process origin lock.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `LogKilledProcessOriginLock` method to log the killed process origin lock.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `CreateHandle` method to create a handle for a child process.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `AddProcessReference` method to add a process reference.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `RemoveProcessReference` method to remove a process reference.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `IsWebSafeScheme` method to check if a scheme is web-safe.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `IsPseudoScheme` method to check if a scheme is a pseudo scheme.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `ClearRegisteredSchemeForTesting` method to clear a registered scheme for testing purposes.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `RegisterWebSafeScheme` method to register a web-safe scheme.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `RegisterWebSafeIsolatedScheme` method to register a web-safe isolated scheme.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `RegisterPseudoScheme` method to register a pseudo scheme.
    -   The `ChildProcessSecurityPolicyImpl` class uses the `DetermineOriginAgentClusterIsolation` method to determine the origin agent cluster isolation.

### Further Investigation

-   The logic for granting and revoking permissions for different types of resources.
-   The interaction between the security policy and other site isolation mechanisms.
-   The impact of incorrect permission handling on cross-origin communication and data access.
-   The logic within `ChildProcessSecurityPolicyImpl::CanCommitURL` to ensure that URLs are correctly checked before being committed.
-   The logic within `ChildProcessSecurityPolicyImpl::CanRequestURL` to ensure that URLs are correctly checked before being requested.
-   The logic within `ChildProcessSecurityPolicyImpl::HasPermissionsForFile` to ensure that file permissions are correctly checked.
-   The logic within `ChildProcessSecurityPolicyImpl::HasPermissionsForFileSystemFile` to ensure that file system permissions are correctly checked.
-   The logic within `ChildProcessSecurityPolicyImpl::AddOriginIsolationStateForBrowsingInstance` to ensure that origin isolation states are correctly added for browsing instances.
-   The logic within `ChildProcessSecurityPolicyImpl::AddFutureIsolatedOrigins` to ensure that future isolated origins are correctly added.
-   The logic within `ChildProcessSecurityPolicyImpl::AddIsolatedOriginInternal` to ensure that isolated origins are correctly added.
-   The logic within `ChildProcessSecurityPolicyImpl::RemoveOptInIsolatedOriginsForBrowsingInstanceInternal` to ensure that opt-in isolated origins are correctly removed for a browsing instance.
-   The logic within `ChildProcessSecurityPolicyImpl::ClearBrowserContextIfMatches` to ensure that the browser context is correctly cleared if it matches a given browser context.
-   The logic within `ChildProcessSecurityPolicyImpl::GetSecurityState` to get the security state for a child process.
-   The logic within `ChildProcessSecurityPolicyImpl::GetKilledProcessOriginLock` to get the killed process origin lock.
-   The logic within `ChildProcessSecurityPolicyImpl::LogKilledProcessOriginLock` to log the killed process origin lock.
-   The logic within `ChildProcessSecurityPolicyImpl::CreateHandle` to create a handle for a child process.
-   The logic within `ChildProcessSecurityPolicyImpl::AddProcessReference` to add a process reference.
-   The logic within `ChildProcessSecurityPolicyImpl::RemoveProcessReference` to remove a process reference.
-   The logic within `ChildProcessSecurityPolicyImpl::IsWebSafeScheme` to check if a scheme is web-safe.
-   The logic within `ChildProcessSecurityPolicyImpl::IsPseudoScheme` to check if a scheme is a pseudo scheme.
-   The logic within `ChildProcessSecurityPolicyImpl::ClearRegisteredSchemeForTesting` to clear a registered scheme for testing purposes.
-   The logic within `ChildProcessSecurityPolicyImpl::RegisterWebSafeScheme` to register a web-safe scheme.
-   The logic within `ChildProcessSecurityPolicyImpl::RegisterWebSafeIsolatedScheme` to register a web-safe isolated scheme.
-   The logic within `ChildProcessSecurityPolicyImpl::RegisterPseudoScheme` to register a pseudo scheme.
-   The logic within `ChildProcessSecurityPolicyImpl::DetermineOriginAgentClusterIsolation` to determine the origin agent cluster isolation.

### Files Analyzed:
* `chromiumwiki/README.md`
* `content/browser/url_info.cc`
* `chromiumwiki/url_info.md`
* `content/browser/site_info.cc`
* `chromiumwiki/site_info.md`
* `content/browser/site_instance_impl.cc`
* `chromiumwiki/site_instance.md`
* `content/browser/site_instance_group.cc`
* `chromiumwiki/site_instance_group.md`
* `content/browser/renderer_host/render_process_host_impl.cc`
* `chromiumwiki/render_process_host.md`
* `content/browser/renderer_host/navigation_request.cc`
* `chromiumwiki/navigation_request_core.md`
* `content/browser/child_process_security_policy_impl.cc`
* `chromiumwiki/child_process_security_policy_impl.md`
