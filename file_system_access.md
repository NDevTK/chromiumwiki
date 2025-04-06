# Component: File System Access API

## 1. Component Focus
*   **Functionality:** Implements the File System Access API ([Spec](https://wicg.github.io/file-system-access/)), allowing web applications to interact directly with files and directories on the user's local device after explicit user permission grant via picker UIs.
*   **Key Logic:** Handling user gestures for picker initiation (`showOpenFilePicker`, `showSaveFilePicker`, `showDirectoryPicker`), managing permissions (`FileSystemAccessPermissionContext`), representing file/directory handles (`FileSystemAccessFileHandleImpl`, `FileSystemAccessDirectoryHandleImpl`), performing file operations (read, write, create), managing locks (`FileSystemAccessLockManager`), transferring handles (`createFileSystemAccessDataTransferToken`).
*   **Core Files:**
    *   `content/browser/file_system_access/file_system_access_manager_impl.cc`/`.h`: Main browser-side manager.
    *   `content/browser/file_system_access/file_system_access_file_handle_impl.cc`/`.h`
    *   `content/browser/file_system_access/file_system_access_directory_handle_impl.cc`/`.h`
    *   `content/browser/file_system_access/file_system_access_permission_context_impl.cc`/`.h`
    *   `content/browser/file_system_access/file_system_access_lock_manager.cc`/`.h`
    *   `third_party/blink/renderer/modules/file_system_access/` (Renderer-side API implementation)
    *   `ui/shell_dialogs/select_file_dialog.h` (Picker UI interface)

## 2. Potential Logic Flaws & VRP Relevance
*   **Information Leaks via Pickers:** Flaws in how picker dialogs handle suggested names or default paths, potentially leaking environment variables or other sensitive information.
    *   **VRP Pattern (Environment Variable Leak):** `showSaveFilePicker`'s `suggestedName` option could be abused with patterns like `%USERNAME%` or other environment variables. The chosen filename (after variable expansion by the OS file picker) was revealed back to the site, leaking the variable's value. (VRP: `1247389`, `1310461`, `1322058`, `1310462`; VRP2.txt#1102, #2980). Also exploitable via `chrome.downloads.download` with `saveAs: true` (VRP2.txt#2935) and `chrome.downloads.onDeterminingFilename` (VRP2.txt#3189).
*   **Permission Bypass/Scope Issues:** Circumventing user-granted permissions or accessing files/directories outside the intended scope.
    *   **VRP Pattern (Symlink Handling):** Insufficient handling of symbolic links within selected directories (`showDirectoryPicker`, `<input type="file" webkitdirectory>`). If a symlink points outside the chosen directory, the API might incorrectly grant access to linked files/directories. (VRP: `1378484` - symlink to file, VRP2.txt#10231 - symlink to directory). Handling of `.url` shortcut files as links (VRP: `1303486`).
*   **Interaction with Other Features:** Unintended consequences when FSA interacts with other components like Downloads or Extensions.
    *   **VRP Pattern (Extension File Read via FSA+Downloads):** Extensions with only `downloads` permission could read local files by: 1) using `showSaveFilePicker` to get a handle to a predictably named file (e.g., "History") in Downloads, 2) triggering a download of a sensitive local file (e.g., `file:///.../History`), 3) using `chrome.downloads.onDeterminingFilename` to *overwrite* the file handle from step 1 with the sensitive content, 4) reading the content via the still-valid FSA handle. (VRP: `1428743`, VRP2.txt#4610). See [downloads.md](downloads.md).
*   **Silent File Overwrite:** Interactions allowing files to be saved or overwritten without explicit user confirmation via the picker.
    *   **VRP Pattern (Save Picker + Enter Key):** Holding Enter key after initiating `showSaveFilePicker` could lead to silent/quick file save/overwrite in the default directory, bypassing the intended user confirmation step in the picker. (VRP: `1243802`, VRP2.txt#9302). See [input.md](input.md).
*   **Path Traversal:** Standard path traversal risks if user input or API parameters related to paths are not properly validated/sanitized (though less likely given the handle-based approach).
*   **Policy Bypass:** Circumventing enterprise policies restricting file system access. (`FileSystemAccessWriteBlockedForUrls` policy).

## 3. Further Analysis and Potential Issues
*   **Picker Behavior (`show*Picker`):** Analyze the implementation of file/directory pickers (`SelectFileDialog`). How are `suggestedName` and `startIn` handled? How can variable expansion be prevented in suggested names? (VRP: `1247389`). How are interaction requirements (user gesture) enforced? Can the picker UI be spoofed or manipulated? Is the fix for the Enter key bypass (VRP: `1243802`) robust?
*   **Symlink Resolution:** How does the API handle symlinks when traversing directories or accessing file handles obtained via directory pickers? `FileSystemAccessManagerImpl::ResolveDirectory` seems relevant. Are checks sufficient to prevent escaping the granted directory scope? (VRP: `1378484`, VRP2.txt#10231).
*   **Permission Model (`FileSystemAccessPermissionContext`):** How are permissions granted (per-handle, per-origin)? How long do they persist? Can permissions be escalated or bypassed?
*   **Handle Management & Transfer:** Analyze the creation (`CreateFileHandle`, `CreateDirectoryHandle`), serialization (`SerializeHandle`, `DeserializeHandle`), and transfer (`CreateFileSystemAccessDataTransferToken`, `GetEntryFromDataTransferToken`) of handles. Are there lifetime issues or possibilities for handle manipulation?
*   **Locking (`FileSystemAccessLockManager`):** Analyze the file locking mechanism (`TakeLock`, `IsContentious`) for potential race conditions or deadlocks.
*   **Enterprise Policy Enforcement:** How is the `FileSystemAccessWriteBlockedForUrls` policy checked (`OnCheckPathsAgainstEnterprisePolicy`)? Can it be bypassed?

## 4. Code Analysis
*   `FileSystemAccessManagerImpl`: Main browser-process manager.
    *   `ChooseEntries`: Initiates the file/directory picker UI. Needs secure handling of `options` like `suggestedName`, `startIn`. (VRP: `1247389`).
    *   `ResolveDirectory`: Involved in directory traversal, needs robust symlink checks. (Related to VRP: `1378484`, VRP2.txt#10231).
    *   `CreateFileHandle`, `CreateDirectoryHandle`: Create the handle objects.
    *   `SerializeHandle`, `DeserializeHandle`: Handle transfer/persistence.
*   `FileSystemAccessPermissionContext`: Manages permission grants and checks.
*   `FileSystemAccessLockManager`: Handles file locks.
*   `SelectFileDialog` (ui/shell_dialogs): Abstract interface for platform file pickers. Platform implementations need checking for `suggestedName` handling.
*   `<input type="file" webkitdirectory>`: Related file system interaction, vulnerable to symlink issues (VRP: `1378484`, VRP2.txt#10231). Check `FileChooserImpl::EnqueueSelectedDirectory`.
*   `DownloadsApi::DetermineFilenameFunction`: Interaction point for the extension download+FSA exploit chain (VRP: `1428743`).

## 5. Areas Requiring Further Investigation
*   **Symlink Handling:** Thoroughly audit all code paths involving directory traversal or file access via handles derived from directory access (e.g., `ResolveDirectory`, `GetEntry`) to ensure symlinks pointing outside the granted scope are correctly blocked. Test across platforms.
*   **Picker Parameter Sanitization:** Ensure `suggestedName` in `showSaveFilePicker` cannot be used to leak environment variables or other sensitive information via OS-level expansion. Block dangerous characters (`%` on Windows?).
*   **Interaction Delays for Pickers:** Verify if pickers initiated via `show*Picker` have adequate protection against clickjacking/keyjacking similar to permission/autofill prompts (related to VRP: `1243802`).
*   **Handle Lifetime/Revocation:** How are permissions revoked when tabs close or contexts change? Are handle lifetimes managed securely?
*   **Cross-Component Interactions:** Continue investigating interactions with Downloads API (VRP: `1428743`) and potentially other features like Drag and Drop.

## 6. Related VRP Reports
*   **Environment Variable Leaks:** VRP: `1247389`, `1310461`, `1322058`, `1310462`; VRP2.txt#1102 (via `showSaveFilePicker`), #2935 (via `chrome.downloads`), #2980 (via Chrome Apps `chooseEntry`).
*   **Symlink Issues:** VRP: `1378484` (Link to file via `<input webkitdirectory>`), VRP2.txt#10231 (Link to directory via `<input webkitdirectory>`).
*   **Download Interaction:** VRP: `1428743` / VRP2.txt#4610 (Extension reading local files via Downloads + FSA interaction).
*   **Silent Save/Overwrite:** VRP: `1243802` / VRP2.txt#9302 (`showSaveFilePicker` + Enter key).
*   **`.url` File Handling:** VRP: `1303486` / VRP2.txt#12993 (Uploading `.url` file could lead to arbitrary file read via picker).

*(See also [downloads.md](downloads.md), [input.md](input.md))*
