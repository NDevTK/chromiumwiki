# Component: File System Access API

## 1. Component Focus
*   **Functionality:** Implements the File System Access API ([Spec](https://wicg.github.io/file-system-access/)), allowing web applications to interact directly with files and directories on the user's local device after explicit user permission grant via picker UIs.
*   **Key Logic:** Handling user gestures for picker initiation (`showOpenFilePicker`, `showSaveFilePicker`, `showDirectoryPicker`), managing permissions (`FileSystemAccessPermissionContext`), representing file/directory handles (`FileSystemAccessFileHandleImpl`, `FileSystemAccessDirectoryHandleImpl`), performing file operations (read, write, create), managing locks (`FileSystemAccessLockManager`), transferring handles (`createFileSystemAccessDataTransferToken`).
*   **Core Files:**
    *   `content/browser/file_system_access/file_system_access_manager_impl.cc`/`.h`: Main browser-side manager. Handles picker initiation (`ChooseEntries`, `SetDefaultPathAndShowPicker`).
    *   `content/browser/file_system_access/file_system_access_file_handle_impl.cc`/`.h`
    *   `content/browser/file_system_access/file_system_access_directory_handle_impl.cc`/`.h`: Implements directory handle operations like `GetFileHandle`, `GetDirectoryHandle`.
    *   `content/browser/file_system_access/file_system_access_permission_context_impl.cc`/`.h`
    *   `storage/browser/file_system/file_system_operation_impl.cc`: Implements underlying file operations like `DirectoryExists`, `CreateDirectory`.
    *   `storage/browser/file_system/async_file_util_adapter.cc`: Adapts sync file utils.
    *   `storage/browser/file_system/native_file_util.cc`: Platform-agnostic native file utilities. Calls `base::GetFileInfo`.
    *   `base/files/file_util_win.cc`, `base/files/file_util_posix.cc`: Platform-specific file utilities like `GetFileInfo`.

## 2. Potential Logic Flaws & VRP Relevance
*   **Information Leaks via Pickers:**
    *   **VRP Pattern (Environment Variable Leak - MITIGATED):** `showSaveFilePicker`'s `suggestedName`. **Mitigated by `%` replacement.** (VRP: `1247389`, etc.; VRP2.txt#1102, #2980).
*   **Permission Bypass/Scope Issues:**
    *   **VRP Pattern (Symlink Handling - POSIX?):** Insufficient handling of symbolic links. When `FileSystemAccessDirectoryHandleImpl::GetDirectoryHandle` is called on a symlink name:
        1. `GetDirectoryResolved` calls `FileSystemOperationRunner::DirectoryExists`.
        2. ... which calls `FileSystemOperationImpl::DirectoryExists`.
        3. ... which calls `AsyncFileUtil::GetFileInfo`.
        4. ... which likely calls `NativeFileSystemFileUtil::GetFileInfo`.
        5. ... which calls `base::GetFileInfo`.
        6. **On Windows:** `base::GetFileInfo` uses `GetFileAttributesExW`, which **does not follow symlinks**. `DidDirectoryExists` correctly returns `FILE_ERROR_NOT_A_DIRECTORY`.
        7. **On POSIX:** `base::GetFileInfo` likely uses `stat`, which **follows symlinks by default**. If the target is a directory, `DidDirectoryExists` returns `FILE_OK`.
        8. `GetDirectoryResolved` **lacks a subsequent check to validate if the resolved path (after following the symlink) is still within the scope** of the original directory handle's permission grant.
        9. A new directory handle is created for the symlink's target path, potentially outside the granted scope.
        **(VRP: `1378484` - symlink via `<input webkitdirectory>`, VRP2.txt#10231 - symlink via `GetDirectoryHandle`).** Note: `GetFileHandle` *does* have a `ConfirmSensitiveEntryAccess` check which might mitigate this for files, but it appears missing for directories. Handling of `.url` shortcut files as links (VRP: `1303486`) needs separate verification.
*   **Interaction with Other Features:**
    *   **VRP Pattern (Extension File Read via FSA+Downloads):** (VRP: `1428743`, VRP2.txt#4610). See [downloads.md](downloads.md).
*   **Silent File Overwrite:**
    *   **VRP Pattern (Save Picker + Enter Key):** (VRP: `1243802`, VRP2.txt#9302). See [input.md](input.md).
*   **Policy Bypass:** (`FileSystemAccessWriteBlockedForUrls`).

## 3. Further Analysis and Potential Issues
*   **Picker Behavior (`show*Picker`):** UI interactions, gesture requirements, Enter key bypass fix (VRP: `1243802`).
*   **Symlink Resolution & Scope Checks (POSIX):** **Confirm `base::GetFileInfo` (via `stat`) follows symlinks on POSIX.** If so, `GetDirectoryResolved` needs scope validation after `GetFileInfo` returns success for a symlink pointing to a valid directory. Audit similar paths like `<input webkitdirectory>`.
*   **Permission Model (`FileSystemAccessPermissionContext`):** Persistence, escalation.
*   **Handle Management & Transfer:** Lifetime, manipulation.
*   **Locking (`FileSystemAccessLockManager`):** Race conditions.
*   **Enterprise Policy Enforcement:** Bypasses.

## 4. Code Analysis
*   `FileSystemAccessManagerImpl`: Main manager. `ChooseEntries`, `SetDefaultPathAndShowPicker`.
*   `FileSystemAccessDirectoryHandleImpl`: Implements directory ops.
    *   `GetDirectoryResolved`: **Lacks scope validation after symlink resolution on POSIX.**
    *   `GetFileResolved`: *Includes* `ConfirmSensitiveEntryAccess` check.
*   `FileSystemAccessPermissionContext`: Manages permissions.
*   `FileSystemOperationImpl`: Implements file ops. `DirectoryExists` uses `AsyncFileUtil::GetFileInfo`.
*   `AsyncFileUtilAdapter`: Wraps sync utils.
*   `NativeFileSystemFileUtil`: Calls `base::GetFileInfo`.
*   `base::GetFileInfo` (`file_util_win.cc`, `file_util_posix.cc`): Platform-specific metadata retrieval. **Win version does not follow symlinks; POSIX likely does.**
*   `SelectFileDialog` / `FileChooserImpl`: Picker UI / `<input>`.

## 5. Areas Requiring Further Investigation
*   **Symlink Handling (`GetDirectoryResolved` - POSIX):** **Confirm `base::GetFileInfo` follows symlinks on POSIX.** Add scope validation checks if needed.
*   **Picker Parameter Sanitization:** Other `suggestedName` issues?
*   **Interaction Delays for Pickers:** Clickjacking/keyjacking protection (VRP: `1243802`).
*   **Handle Lifetime/Revocation.**
*   **Cross-Component Interactions:** Downloads API (VRP: `1428743`), Drag/Drop.

## 6. Related VRP Reports
*   **Environment Variable Leaks (Mitigated for FSA):** VRP: `1247389`, etc.; VRP2.txt#1102, #2980.
*   **Symlink Issues:** VRP: `1378484`, VRP2.txt#10231 (**Likely POSIX-specific due to lack of scope check after symlink is followed in `GetDirectoryHandle` flow**).
*   **Download Interaction:** VRP: `1428743` / VRP2.txt#4610.
*   **Silent Save/Overwrite:** VRP: `1243802` / VRP2.txt#9302.
*   **`.url` File Handling:** VRP: `1303486` / VRP2.txt#12993.

*(See also [downloads.md](downloads.md), [input.md](input.md))*
