# Component: Installer &amp; Updater (Windows/macOS)

## 1. Component Focus
*   **Functionality:** Handles the installation, update, and uninstallation processes for Chrome/Chromium. These operations frequently run with elevated privileges (SYSTEM on Windows, root on macOS) to write to protected locations (`Program Files`, `/Library`, registry hives `HKLM`). Key components include `setup.exe`, `msiexec` (via MSI packages), Google Update service (Omaha - `GoogleUpdate.exe`, `goopdate.dll`), Keystone framework (macOS - `ksinstall`), Chrome Elevation Service, and potentially Crashpad during setup.
*   **Key Logic:** Package extraction, file copying/deletion/moving, directory creation, ACL management, registry manipulation (Windows), service registration/management, version checking, signature verification, IPC/COM/XPC communication for coordination and triggering updates.
*   **Core Files:**
    *   `chrome/installer/setup/` (`setup_main.cc`, `install.cc`, `install_worker.cc`, `uninstall.cc`)
    *   `chrome/installer/util/` (`logging_installer.cc`, `util_constants.cc`, `work_item.h`, `work_item_list.h`, `move_tree_work_item.cc`, `copy_tree_work_item.cc`, `delete_old_versions.cc`, `registry_util.cc`)
    *   `base/files/file_util_win.cc`: Contains `internal::MoveUnsafe` (implementation for `base::Move`), `DeletePathRecursively`.
    *   `base/win/registry.cc`: Wrapper for Windows registry APIs.
    *   `google_update/` (Omaha source - Windows) (`goopdate/` especially `GoogleUpdate.exe`, `service/`, `common/`, `omaha/`, `goopdate/com_proxy.cc`, `google_update_idl.idl`)
    *   Keystone framework source (macOS - potentially separate repository, involves `ksinstall`, `Keystone.tbz`)
    *   `chrome/elevation_service/` (Windows)
    *   `components/crashpad/` (Crashpad client/handler setup integration)

## 2. Potential Logic Flaws & VRP Relevance

Installer and updater components are prime targets for Elevation of Privilege (EoP) vulnerabilities due to their need to run with high privileges while interacting with potentially user-controllable inputs or environments (temporary folders, registry keys, update channels).

*   **Insecure File/Directory Operations (High EoP Risk):** Creating, deleting, moving files, or setting permissions in locations accessible or influenceable by lower-privileged users, without adequate checks for symbolic links, junctions, hardlinks, or race conditions.
    *   **VRP Pattern (Temp/Log Dirs Abuse):** Elevated processes performing operations in world-writable or user-writable temporary/log locations (`C:\Windows\Temp`, `C:\ProgramData\Google\Update\Log`, `%APPDATA%\Local`, `/tmp/com.google.Keystone`) allowing link planting. This often involves TOCTOU vulnerabilities.
        *   *Examples:* `setup.exe` logging to `C:\Windows\Temp` (VRP2.txt#1333), Crashpad creating dirs/files in `C:\Windows\Temp` (VRP2.txt#9914), Google Update logging to `C:\ProgramData` (VRP2.txt#1191, #4151), Keystone lock file in `/tmp` (VRP2.txt#914), Uninstaller moving `setup.exe` to `C:\Windows\Temp` (VRP2.txt#12876), Scoped directory deletion in `C:\Windows\Temp` (VRP2.txt#1259). Exploits often use `ReadDirectoryChangesW` to predict temp names (VRP2.txt#1259).
    *   **VRP Pattern (Installer MoveTree TOCTOU):** The primary installation step involves moving the unpacked version directory from a temporary location (`%ProgramFiles%\Google\Temp\*` or `%LOCALAPPDATA%\Google\Temp\*`) to the final installation directory (`%ProgramFiles%\Google\Chrome\Application\*`) using `MoveTreeWorkItem`. This internally calls `base::Move`, which on Windows uses `MoveFileExW` *without* the `MOVEFILE_FAIL_IF_NOT_TRACKABLE` flag. This creates a TOCTOU window: an attacker can replace the source temporary directory with a symlink/junction *after* initial checks but *before* the `MoveFileExW` call. `MoveFileExW` might then follow the link and move/overwrite a sensitive system directory/file into the privileged destination, leading to EoP. (Related VRPs likely leverage this, e.g., VRP: `1317661`, `1183137`).
    *   **VRP Pattern (Recursive Deletion Link Following):** The background process (`setup.exe --delete-old-versions`) and the uninstaller (`UninstallProduct`) use `base::DeletePathRecursively` to remove old version directories or the entire installation directory. This function does not appear to use link-safe flags (`FILE_FLAG_OPEN_REPARSE_POINT`) consistently during its checks or underlying API calls (`GetFileAttributes`, `DeleteFile`, `RemoveDirectory`). If an attacker replaces a directory/file targeted for deletion with a symlink/junction pointing to a sensitive system location *before* the elevated deletion occurs, the recursive delete might follow the link, leading to arbitrary file/directory deletion (EoP).
    *   **VRP Pattern (MSI Repair EoP):** Standard user triggers MSI repair via `msiexec`, Windows Installer Service (SYSTEM) writes/executes temp files (`Google Update Setup`). Vulnerable if temp path logic (`GetTempFileName`) can be manipulated (e.g., by locking files) to force writes to user-controlled `%TEMP%` (VRP2.txt#139).
    *   **VRP Pattern (File ACLs):** Incorrectly setting permissive ACLs on files/directories created by elevated processes, allowing tampering by standard users (VRP2.txt#4151, #1191 - Google Update logs). `AddInstallWorkItems` attempts some ACL setting (`ConfigureAppContainerSandbox`, granting user access to histogram dir) - check robustness.

*   **Race Conditions (TOCTOU):** Time-of-check-to-time-of-use flaws, often involving file system or registry operations beyond the primary MoveTree/Delete cases described above.
    *   **VRP Pattern (File Ops Races):** Races between checking file properties (existence, type, size, reparse point status) and performing operations (write, delete, move, set permissions). Often exploitable using oplocks.
        *   *Examples:* Google Update logging race between `CreateFileW` and `IsReparsePoint` check (VRP2.txt#1191). MSI repair temp file locking race (VRP2.txt#139).
    *   **VRP Pattern (Update Logic Races - macOS):** Keystone race condition between checking the version inside an update package (`.tbz`) and verifying the code signature *after* extraction, allowing substitution of the package content mid-process (VRP2.txt#914).

*   **Insecure Linking Vulnerabilities:** Failure to securely handle symbolic links, junctions (Windows), or hardlinks (macOS) when performing privileged file/registry operations. This is the core mechanism exploited by many TOCTOU bugs.
    *   **VRP Pattern (File Link Following):** Elevated process follows a planted link to operate on an unintended target file/directory. Robust checks (`FILE_FLAG_OPEN_REPARSE_POINT`, `O_NOFOLLOW`, checking target attributes) or API flags (`MOVEFILE_FAIL_IF_NOT_TRACKABLE`) are needed *before* the operation. The lack of appropriate checks/flags in `base::Move` and `base::DeletePathRecursively` is a key weakness.
        *   *Examples:* Google Update log rotation (`File::Move` without checks - VRP2.txt#1191), Crashpad temp file creation (VRP2.txt#9914), Installer logging (VRP2.txt#1333), Scoped temp dir deletion (VRP2.txt#1259), Uninstaller moving setup.exe (VRP2.txt#12876). Keystone lock file creation (macOS hardlink bypass - VRP2.txt#914). Keystone `.tbz` extraction (symlink inside archive - VRP2.txt#914). `MoveTreeWorkItem` TOCTOU (see above). `DeleteOldVersions`/`DeletePathRecursively` (see above).
    *   **VRP Pattern (Registry Link Following):** Uninstaller deleting keys (e.g., HKCU Active Setup keys via `VisitUserHives`) in user-writable locations. Attacker replaces key with a registry symlink pointing to a protected key (e.g., HKLM policies). The elevated delete operation (`DeleteRegistryKey` -> `RegDeleteKeyEx`) typically *doesn't* follow links itself. The vulnerability likely exploits a TOCTOU window where a *check* (e.g., in `DeleteRegistryKeyIf`) reads the link target before the delete operation acts on the (original) link path, or exploits the hive loading/iteration logic in `VisitUserHives`. (VRP2.txt#8739).

*   **Insecure IPC/COM/XPC Handling:** Interfaces exposed by elevated services lacking proper authentication, authorization, or impersonation.
    *   **VRP Pattern (Windows COM Session Moniker):** `GoogleUpdate.ProcessLauncher` COM object callable by standard users allows launching commands in *other* user sessions via Session Monikers, potentially targeting an administrator session (VRP2.txt#3763).
    *   **VRP Pattern (Missing Impersonation - Windows COM):** Google Update `IAppBundleWeb->download` calls `DownloadManager::CachePackage` which uses `CallAsSelfAndImpersonate2` but subsequent file copy (`CopyFile`) might lack impersonation, allowing arbitrary file *read* by planting a junction in user's temp dir before the copy (VRP2.txt#1152).

*   **Insufficient Validation/Checks:** Other validation failures beyond linking or IPC.
    *   **VRP Pattern (Signature/Version Bypass):** Keystone skipping code signing verification if no previous version is installed or during race conditions (VRP2.txt#914). CRX3 signature bypass via embedded ZIP64 payload (relevant if installer uses CRX3 directly? VRP2.txt#3063).
    *   **VRP Pattern (Environment Variables):** Using user-controlled environment variables (like `%TEMP%`) in elevated processes when secure paths fail (VRP2.txt#139).

## 3. Further Analysis and Potential Issues
*   **File System Operations & ACLs:** Deep dive into all privileged file operations (`CreateFileW`, `DeleteFileW`, `MoveFileExW`, `SetFileSecurityW`, directory creation/deletion, archive extraction), especially within `WorkItem` implementations (`MoveTreeWorkItem`, `CopyTreeWorkItem`, `DeleteTreeWorkItem`).
    *   Where are files being created/deleted/moved? Focus on shared locations (`C:\Windows\Temp`, `C:\ProgramData`, `/tmp`, user profiles `%APPDATA%`) and the installer's own temp dir (`%ProgramFiles%\Google\Temp` or `%LOCALAPPDATA%\Google\Temp`).
    *   Are symlink/junction/hardlink checks performed *before* operations, and are they robust? `base::Move` needs `MOVEFILE_FAIL_IF_NOT_TRACKABLE`. How do `CopyTreeWorkItem`, `DeleteTreeWorkItem`, `base::DeletePathRecursively` handle links?
    *   Are ACLs set securely on created directories/files to prevent tampering by lower-privileged users? (See VRP2.txt#4151, #1191). Review `ConfigureAppContainerSandbox`.
    *   Can temporary filenames be predicted or monitored (`ReadDirectoryChangesW`)? (VRP2.txt#1259).
    *   Are oplocks considered in TOCTOU scenarios? (VRP2.txt#1191, #139).
*   **Log File Handling (`GoogleUpdate.ini`, `FileLogWriter`):** Although the `LogFilePath` issue (VRP2.txt#1363) is likely fixed, the entire logging mechanism (`FileLogWriter::CreateLoggingFile`, `FileLogWriter::ArchiveLoggingFile`) is sensitive due to running as SYSTEM and potentially writing to `C:\ProgramData\Google\Update\Log`. Review ACLs, file rotation logic (TOCTOU, VRP2.txt#1191), and handling of `MaxLogFileSize`. Re-check for reparse point vulnerabilities.
*   **IPC/COM/XPC Interfaces:** Identify all interfaces exposed by elevated services (Google Update, Keystone, Elevation Service). Audit methods for authorization checks (caller identity, permissions). Check for Session Moniker vulnerabilities (Windows, VRP2.txt#3763). Verify correct impersonation (`ImpersonateLoggedOnUser`, `RevertToSelf`, `CallAsSelfAndImpersonate2`) before performing actions, especially file operations (VRP2.txt#1152).
*   **Registry Operations (Windows):** Identify all registry keys modified/deleted by installer/uninstaller running with elevated privileges. If any key resides in a location modifiable by standard users (e.g., under HKCU), assess vulnerability to registry symlink attacks (VRP2.txt#8739). Examine `DeleteRegistryKeyIf` and `VisitUserHives` interaction closely.
*   **Update Package Handling (`.msi`, `.pkg`, `.tbz`):** Review archive extraction logic for path traversal and link vulnerabilities. Ensure signature/version verification happens *after* extraction to secure locations and *before* using the extracted files. Analyze race conditions between version checks and signature checks (VRP2.txt#914).
*   **Uninstallation Logic:** Review `uninstall.cc` and related platform-specific cleanup routines for insecure file/registry deletions in user-writable locations (VRP2.txt#8739, #12876).
*   **Crashpad Integration:** Analyze file operations performed by Crashpad setup/handler when invoked by the installer (VRP2.txt#9914).

## 4. Code Analysis
*   **Windows File Operations:**
    *   `chrome/installer/setup/setup_main.cc`: Entry point, handles command line args, singleton lock, delegates install/uninstall. Creates temp directories (`CreateTemporaryAndUnpackDirectories`), launches child processes (e.g., for MSI DisplayVersion update, shortcuts, old version deletion). Calls `RepeatDeleteOldVersions`.
    *   `chrome/installer/setup/install_worker.cc`: Contains `AddInstallWorkItems`, `AppendPostInstallTasks`. Orchestrates adding `WorkItem`s for file copying, registry, shortcuts, etc. Calls `AddChromeWorkItems`.
    *   `chrome/installer/util/move_tree_work_item.cc`: Implements `MoveTreeWorkItem::DoImpl`. Checks destination existence, optionally checks for duplicates (`IsIdenticalFileHierarchy`), backs up destination to temp dir, calls `base::Move`. Rollback attempts reversal. **Vulnerable to TOCTOU source symlink attack due to `base::Move` lacking `MOVEFILE_FAIL_IF_NOT_TRACKABLE`.**
    *   `chrome/installer/util/copy_tree_work_item.cc`: Implements copying. Backs up destination via `base::Move`. Copies source via `base::CopyDirectory`. **Potentially vulnerable to TOCTOU on backup move and source copy.**
    *   `chrome/installer/util/delete_old_versions.cc`: Contains `DeleteOldVersions` logic called by background process. Calls `DeleteVersion`, which tries to lock files before calling `base::DeleteFile` and `base::DeletePathRecursively`. **`DeletePathRecursively` might follow symlinks.**
    *   `base/files/file_util_win.cc`: Implements `internal::MoveUnsafe` (backend for `base::Move`). Uses `MoveFileExW` with `MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING` but **lacks `MOVEFILE_FAIL_IF_NOT_TRACKABLE`**. Implements `DeletePathRecursively` using `GetFileAttributes` (doesn't use `FILE_FLAG_OPEN_REPARSE_POINT`), `DeleteFile`, `RemoveDirectory`. **Lacks explicit symlink/junction checks during recursive deletion.**
    *   `chrome/installer/util/logging_installer.cc`: Potentially writes `chrome_installer.log` / `chromium_installer.log` to `DIR_TEMP` (which is `C:\Windows\Temp` when run as SYSTEM) (VRP2.txt#1333). Uses `base::CreateDirectory` and `logging::InitLogging`.
    *   `google_update/omaha/base/logging.cc` (`goopdate`): `FileLogWriter::CreateLoggingFile`, `FileLogWriter::ArchiveLoggingFile`. Check `CreateFileW` usage, ACL setting (`SetDacl`), `IsReparsePoint` checks, move logic. (VRP2.txt#1191, #4151).
    *   `google_update/omaha/goopdate/download_manager.cc` (`goopdate`): `CachePackage` uses `CallAsSelfAndImpersonate2` before `CopyFile` - check for missing impersonation leading to arbitrary file read (VRP2.txt#1152).
    *   `chrome/installer/setup/uninstall.cc`: `MoveSetupOutOfInstallFolder` moves `setup.exe` to temp dir using `base::Move` (VRP2.txt#12876). Calls `DeleteChromeFilesAndFolders` which uses `base::DeletePathRecursively`.
*   **macOS File Operations:**
    *   `ksinstall` (Keystone): Handles `.tbz` extraction (`/usr/bin/tar -Oxjf`). Needs check for symlinks within archive (VRP2.txt#914).
    *   Keystone framework: Lock file creation (`+[KSInstallSettings userInstallLockFile:]` writing to `/tmp/com.google.Keystone/.keystone_install_lock`). Check `open` flags vs hardlinks (VRP2.txt#914).
*   **Windows Registry Operations:**
    *   `chrome/installer/setup/uninstall.cc`: Deletes Active Setup keys via `DeleteRegistryKey` (defined in `registry_util.cc`). Calls `VisitUserHives`.
    *   `chrome/installer/util/registry_util.cc`: Implements `DeleteRegistryKey`, `DeleteRegistryValue`, `DeleteRegistryKeyIf`. Uses `base::win::RegKey` which wraps standard Windows APIs (`RegOpenKeyEx`, `RegDeleteKeyEx`). **`RegDeleteKeyEx` typically does not follow links on delete**, making VRP#8739 likely related to conditional checks (`DeleteRegistryKeyIf` checking the link target) or hive loading (`VisitUserHives`).
    *   `chrome/installer/util/delete_reg_key_work_item.cc`: Implements `DeleteRegKeyWorkItem` which uses `installer::DeleteRegistryKey`.
*   **Windows IPC/COM:**
    *   `google_update/google_update_idl.idl` (`goopdate`): Defines `IGoogleUpdate3Web`, `IAppBundleWeb`.
    *   `GoogleUpdate.ProcessLauncher` COM class (Check implementation): Exposes `IProcessLauncher` (`LaunchCmdLine`). Vulnerable to Session Moniker attack (VRP2.txt#3763).
    *   `google_update/omaha/goopdate/goopdate_coclass.cc` (implements `IAppBundleWeb`): Check `DownloadPackage` logic -> `DownloadManager::CachePackage` -> `CopyFile` regarding impersonation (VRP2.txt#1152).
    *   `chrome/elevation_service/`: Implements Chrome Elevation Service, check methods like `RunRecoveryCRXElevated` for file handling security.
*   **Signature/Version Checks:**
    *   Keystone (`ksinstall`): Check logic comparing versions before/after extraction and signature validation timing (VRP2.txt#914).
    *   `components/crx_file/crx_verifier.cc`: Verify CRX3 signatures. Check for potential bypasses (VRP2.txt#3063 - ZIP64 embed).

## 5. Areas Requiring Further Investigation
*   **`base::Move` / `base::DeletePathRecursively` Security:** Confirm the exact behavior of underlying Windows APIs (`MoveFileExW`, `DeleteFile`, `RemoveDirectory`, `GetFileAttributes`) regarding symlinks/junctions with the flags used by `base`. The lack of explicit link protection remains the primary concern for file operations.
*   **`CopyTreeWorkItem` TOCTOU:** Further investigate the TOCTOU risks during the backup move and the main copy phases.
*   **Registry Symlink Attack (VRP#8739):** Deep dive into `VisitUserHives` and the exact sequence of operations during Active Setup key deletion to understand how the link following occurs if `RegDeleteKeyEx` itself isn't the vector. Does `RegKey::Open` follow the link in the conditional check (`DeleteRegistryKeyIf`)?
*   **Comprehensive File Op Audit:** Systematically audit *all* privileged file operations (including those in Google Update/Keystone if possible) using the identified patterns.
*   **IPC/COM/XPC Interface Review:** Map out and audit interfaces exposed by elevated services. Verify authentication, authorization, and impersonation for each method.
*   **Package Extraction Security:** Review code handling `.msi`, `.pkg`, `.tbz` extraction for path traversal and link vulnerabilities.
*   **Crashpad Installer Interaction:** Analyze file operations during Crashpad installation triggered by the main installer.

## 6. Related VRP Reports
*   VRP2.txt#139: MSI repair EoP (Temp file locking, `%TEMP%` abuse)
*   VRP2.txt#914: Keystone EoP (Lock file hardlink, codesign/version race, symlink in tbz)
*   VRP2.txt#1191: Google Update Logging EoP (TOCTOU, oplock, `IsReparsePoint` race)
*   VRP2.txt#1259: Scoped Temp Dir Deletion EoP (Junction planting, `ReadDirectoryChangesW`)
*   VRP2.txt#9914: Crashpad Temp File Creation EoP (Symlink)
*   VRP2.txt#1328: Crashpad Temp File Creation EoP (Symlink - possibly dup of 9914?)
*   VRP2.txt#1363: Google Update Logging EoP (`GoogleUpdate.ini` `LogFilePath` - likely fixed)
*   VRP2.txt#4151: Google Update Logging EoP (Permissive ACLs on log dir + junction)
*   VRP2.txt#8739: Uninstaller Registry EoP (Registry symlink deletion)
*   VRP2.txt#1333: Installer Logging EoP (`chrome_installer.log` in `C:\Windows\Temp` + symlink)
*   VRP2.txt#12876: Uninstaller Temp File EoP (Move setup to temp + symlink)
*   VRP2.txt#1152: Google Update Arbitrary File Read (`IAppBundleWeb->download` cache + missing impersonation)
*   VRP2.txt#3763: Google Update COM EoP (`GoogleUpdate.ProcessLauncher` Session Moniker)
*   VRP2.txt#3063: CRX3 Signature Verification Bypass (Relevant if used by installer?)
*   VRP2.txt#6852: Chrome Elevation Service LPE (Arbitrary file copy) - Not directly installer, but related elevated service.

*(List reviewed against VRP2.txt for Installer/Updater EoP reports >$1000).*