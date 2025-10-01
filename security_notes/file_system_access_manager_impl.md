# FileSystemAccessManagerImpl (`content/browser/file_system_access/file_system_access_manager_impl.h`)

## 1. Summary

The `FileSystemAccessManagerImpl` is the main entry point and security authority in the browser process for the File System Access API. This powerful API allows websites to read, write, and manage files and directories directly on the user's local filesystem.

This class is one of the most security-critical components in the entire browser. A vulnerability here could easily lead to a total compromise of user data, allowing a malicious website to read sensitive files or write malware to the user's disk, completely bypassing the browser sandbox. It orchestrates all aspects of the API, from showing the user a file picker to managing permissions and brokering access to the filesystem.

## 2. Core Concepts

*   **Permission-First Model:** The entire API is built around user consent. A website cannot access any file or directory without first getting explicit permission from the user, typically via a file picker dialog. This class does not show the picker itself, but it initiates the process and receives the result.

*   **`FileSystemAccessPermissionContext`:** The manager does not make permission decisions itself. It delegates all permission checks to the `FileSystemAccessPermissionContext`. This is a crucial security abstraction, allowing the embedder (Chrome) to plug in its own logic for remembering, managing, and revoking permissions.

*   **Handle-Based Access:** Once a user grants permission for a file or directory, the manager creates a `FileSystemAccessFileHandleImpl` or `FileSystemAccessDirectoryHandleImpl`. These handles are Mojo objects that are passed to the renderer. They act as capability tokens: possession of a handle is proof that the renderer has permission to operate on the corresponding file or directory.

*   **Path Validation and Sanitization:** A core security responsibility is ensuring that a website can never escape the boundaries of the file or directory it was granted access to. This involves rigorous path validation to prevent path traversal attacks (e.g., using `..` to move up the directory tree).

*   **Sandboxed vs. Native Filesystem:** The manager handles two distinct types of filesystems:
    1.  **Sandboxed Filesystem (`GetSandboxedFileSystem`):** This is the traditional, origin-scoped virtual filesystem. It is safe by default as it is partitioned by origin and not directly accessible by other applications on the user's system.
    2.  **Native Filesystem (`ChooseEntries`):** This provides access to the user's actual local disk, which is far more dangerous and requires much stricter security checks.

## 3. Security-Critical Logic & Vulnerabilities

*   **File Picker and Permission Granting:**
    *   **Risk:** The most critical security moment is when the user grants permission via the file picker. A bug that could allow a website to influence the picker's starting directory, pre-select files, or otherwise trick the user into granting broader permissions than intended would be a critical vulnerability.
    *   **Mitigation:** The `ChooseEntries` method is the entry point for this flow. It is responsible for dropping renderer privileges (e.g., exiting fullscreen mode via `ForSecurityDropFullscreen`) before showing the picker, to ensure the user can see the browser's trusted UI and the URL bar.

*   **Path Traversal:**
    *   **Risk:** The single most dangerous vulnerability class for this API is path traversal. If a renderer, after being granted access to `/path/to/user/directory`, could craft a request for `/path/to/user/directory/../../../../etc/passwd`, it could read arbitrary files.
    *   **Mitigation:** The manager and its handle implementations must rigorously validate every path component received from the renderer. The `IsSafePathComponent` method is a key part of this defense, checking for dangerous names or characters. All file operations are ultimately performed by the trusted `storage::FileSystemOperationRunner` in the browser process, which provides another layer of path validation.

*   **Cross-Origin Handle Transfer (`FileSystemAccessTransferToken`):**
    *   **Risk:** The API allows handles to be transferred between different browser contexts (e.g., via `postMessage`). A bug in the token creation (`CreateTransferToken`) or redemption (`ResolveTransferToken`) logic could allow a malicious site to gain access to a handle it was not authorized for.
    *   **Mitigation:** Transfer tokens are `base::UnguessableToken`s, which are cryptographically random and cannot be forged. The manager maintains a map of active tokens (`transfer_tokens_`) and only redeems tokens that it has previously created.

*   **Locking and Race Conditions (`FileSystemAccessLockManager`):**
    *   **Risk:** The API provides file locking to prevent race conditions between different tabs or workers accessing the same file. A flaw in the `FileSystemAccessLockManager` could lead to data corruption if two writers access a file simultaneously.
    *   **Mitigation:** The manager owns the `lock_manager_` and all file write operations must acquire a lock before they can proceed.

## 4. Key Functions

*   `ChooseEntries(...)`: The entry point for showing a file picker to the user, which is the start of any interaction with the native filesystem.
*   `GetSandboxedFileSystem(...)`: The entry point for accessing the origin-scoped sandboxed filesystem.
*   `CreateFileHandle(...)` / `CreateDirectoryHandle(...)`: Factory methods that create the Mojo handles which represent a granted permission. These are passed back to the renderer.
*   `CreateTransferToken(...)` / `ResolveTransferToken(...)`: The secure mechanism for transferring file system access between contexts.
*   `IsSafePathComponent(...)`: A critical security validation function to prevent path traversal attacks.

## 5. Related Files

*   `content/public/browser/file_system_access_permission_context.h`: The interface that must be implemented by the embedder to manage the actual permission grants. This is where permission persistence and revocation logic lives.
*   `content/browser/file_system_access/file_system_access_file_handle_impl.cc` and `file_system_access_directory_handle_impl.cc`: The implementations of the file and directory handles that enforce security checks on every operation.
*   `storage/browser/file_system/file_system_operation_runner.h`: The low-level file I/O runner that actually performs the operations on disk, providing a final layer of path and security validation.
*   `content/browser/file_system_access/file_system_access_lock_manager.h`: The class that manages exclusive and shared locks on file paths to prevent race conditions.