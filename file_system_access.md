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
    *   **VRP Pattern (Symlink Scope Bypass - POSIX):** Insufficient handling of symbolic links on POSIX platforms. When `FileSystemAccessDirectoryHandleImpl::GetDirectoryHandle` is called on a symlink name:
        1. `GetDirectoryResolved` calls `FileSystemOperationRunner::DirectoryExists` -> ... -> `base::GetFileInfo`.
        2. **On POSIX:** `base::GetFileInfo` uses `stat()` which **follows symlinks by default** (confirmed by code search and TODO comment in `base/files/file_util_posix.cc`). If the target is a directory, the check succeeds.
        3. `GetDirectoryResolved` **lacks a subsequent check to validate if the resolved path (after following the symlink) is still within the scope** of the original directory handle's permission grant.
        4. A new directory handle is created for the symlink's target path, allowing access outside the granted scope.
        **(VRP: `1378484` - symlink via `<input webkitdirectory>`, VRP2.txt#10231 - symlink via `GetDirectoryHandle`).** Note: `GetFileHandle` *does* have a `ConfirmSensitiveEntryAccess` check which might mitigate this for files, but it is missing for directories.
*   **Interaction with Other Features:**
    *   **VRP Pattern (Extension File Read via FSA+Downloads):** Combining FSA permissions with `chrome.downloads` API to read arbitrary files (VRP: `1428743`, VRP2.txt#4610). See [downloads.md](downloads.md).
*   **Silent File Overwrite:**
    *   **VRP Pattern (Save Picker + Enter Key):** Using Enter key to confirm save dialog initiated by `showSaveFilePicker` without sufficient interaction delay (VRP: `1243802`, VRP2.txt#9302). See [input.md](input.md).
*   **Policy Bypass:** (`FileSystemAccessWriteBlockedForUrls`).

## 3. Further Analysis and Potential Issues
*   **Picker Behavior (`show*Picker`):** UI interactions, gesture requirements, interaction delay protection (relevant to VRP: `1243802`).
*   **Symlink Resolution & Scope Checks (POSIX):** The core issue is confirmed: `base::GetFileInfo` follows symlinks via `stat()`, and `GetDirectoryResolved` lacks the necessary subsequent scope check. The fix requires adding this validation step. Similar paths like `<input webkitdirectory>` should also be audited.
*   **Permission Model (`FileSystemAccessPermissionContext`):** Persistence, escalation paths, lifetime management.
*   **Handle Management & Transfer:** Lifetime, manipulation potential, postMessage transfer security.
*   **Locking (`FileSystemAccessLockManager`):** Race conditions in acquiring/releasing locks.
*   **Enterprise Policy Enforcement:** Bypasses for `FileSystemAccessWriteBlockedForUrls`, etc.
*   **`.url` File Handling:** How are Windows shortcut files treated? Do they pose similar risks to symlinks? (VRP: `1303486`).

## 4. Code Analysis
*   `FileSystemAccessManagerImpl`: Main manager. `ChooseEntries`, `SetDefaultPathAndShowPicker`.
*   `FileSystemAccessDirectoryHandleImpl`: Implements directory ops.
    *   `GetDirectoryResolved`: Calls `DirectoryExists`. **Crucially lacks scope validation after `DirectoryExists` succeeds for a symlink pointing outside the granted directory on POSIX.**
    *   `GetFileResolved`: *Includes* `ConfirmSensitiveEntryAccess` check (might prevent symlink bypass for files).
*   `FileSystemAccessPermissionContext`: Manages permissions.
*   `FileSystemOperationImpl`: Implements file ops. `DirectoryExists` uses `AsyncFileUtil::GetFileInfo`.
*   `AsyncFileUtilAdapter`: Wraps sync utils.
*   `NativeFileSystemFileUtil`: Calls `base::GetFileInfo`.
*   `base::GetFileInfo` (`file_util_posix.cc`): Platform-specific metadata retrieval. **Confirmed to use `stat()` which follows symlinks.** Contains TODO comment acknowledging need for refactoring symlink handling.
*   `SelectFileDialog` / `FileChooserImpl`: Picker UI / `<input>` elements. Check for interaction bypasses.

## 5. Areas Requiring Further Investigation
*   **Symlink Scope Check Fix (POSIX):** **Implement scope validation check within `FileSystemAccessDirectoryHandleImpl::GetDirectoryResolved` after `DirectoryExists` returns success.** The check should compare the resolved path (obtained perhaps via `base::ReadSymbolicLink` or similar after detecting it's a symlink) against the handle's original granted path.
*   **`.url` File Handling:** Investigate how `.url` files are handled by FSA operations. Are they treated as links? If so, are appropriate scope checks performed? (VRP: `1303486`).
*   **Picker Parameter Sanitization:** Audit suggested name handling and other parameters passed to picker functions.
*   **Interaction Delays for Pickers:** Verify clickjacking/keyjacking protection (`InputEventActivationProtector`?) for `show*Picker` calls (VRP: `1243802`).
*   **Handle Lifetime/Revocation:** Ensure handles are correctly invalidated if permissions are revoked.
*   **Cross-Component Interactions:** Downloads API (VRP: `1428743`), Drag/Drop, other APIs accessing the filesystem.

## 6. Related VRP Reports
*   **Environment Variable Leaks (Mitigated for FSA):** VRP: `1247389`, etc.; VRP2.txt#1102, #2980.
*   **Symlink Scope Bypass (POSIX):** VRP: `1378484`, VRP2.txt#10231 (**Confirmed POSIX-specific due to `stat()` following symlinks and lack of scope check after resolution in `GetDirectoryHandle` flow**).
*   **Download Interaction:** VRP: `1428743` / VRP2.txt#4610.
*   **Silent Save/Overwrite:** VRP: `1243802` / VRP2.txt#9302.
*   **`.url` File Handling:** VRP: `1303486` / VRP2.txt#12993.

## 7. Cross-References
*   [downloads.md](downloads.md)
*   [input.md](input.md)
*   [child_process_security_policy_impl.md](child_process_security_policy_impl.md)
