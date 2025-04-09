# Component: Installer &amp; Updater (Windows/macOS)

## 1. Component Focus
*   **Functionality:** Handles the installation, update, and uninstallation processes for Chrome/Chromium. These operations frequently run with elevated privileges (SYSTEM on Windows, root on macOS) to write to protected locations (`Program Files`, `/Library`, registry hives `HKLM`). Key components include `setup.exe`, `msiexec` (via MSI packages), Google Update service (Omaha - `GoogleUpdate.exe`, `goopdate.dll`), Keystone framework (macOS - `ksinstall`), Chrome Elevation Service, and potentially Crashpad during setup.
*   **Key Logic:** Package extraction, file copying/deletion/moving, directory creation, ACL management, registry manipulation (Windows), service registration/management, version checking, signature verification, IPC/COM/XPC communication for coordination and triggering updates.
*   **Core Files:**
    *   `chrome/installer/setup/` (`setup_main.cc`, `uninstall.cc`)
    *   `chrome/installer/util/` (`logging_installer.cc`, `util_constants.cc`)
    *   `google_update/` (Omaha source - Windows) (`goopdate/` especially `GoogleUpdate.exe`, `service/`, `common/`, `omaha/`, `goopdate/com_proxy.cc`, `google_update_idl.idl`)
    *   Keystone framework source (macOS - potentially separate repository, involves `ksinstall`, `Keystone.tbz`)
    *   `chrome/elevation_service/` (Windows)
    *   `components/crashpad/` (Crashpad client/handler setup integration)

## 2. Potential Logic Flaws &amp; VRP Relevance

Installer and updater components are prime targets for Elevation of Privilege (EoP) vulnerabilities due to their need to run with high privileges while interacting with potentially user-controllable inputs or environments (temporary folders, registry keys, update channels).

*   **Insecure File/Directory Operations (High EoP Risk):** Creating, deleting, moving files, or setting permissions in locations accessible or influenceable by lower-privileged users, without adequate checks for symbolic links, junctions, hardlinks, or race conditions.
    *   **VRP Pattern (Temp/Log Dirs Abuse):** Elevated processes performing operations in world-writable or user-writable temporary/log locations (`C:\Windows\Temp`, `C:\ProgramData\Google\Update\Log`, `%APPDATA%\Local`, `/tmp/com.google.Keystone`) allowing link planting.
        *   *Examples:* `setup.exe` logging to `C:\Windows\Temp` (VRP2.txt#1333), Crashpad creating dirs/files in `C:\Windows\Temp` (VRP2.txt#9914), Google Update logging to `C:\ProgramData` (VRP2.txt#1191, #4151), Keystone lock file in `/tmp` (VRP2.txt#914), Uninstaller moving `setup.exe` to `C:\Windows\Temp` (VRP2.txt#12876), Scoped directory deletion in `C:\Windows\Temp` (VRP2.txt#1259). Exploits often use `ReadDirectoryChangesW` to predict temp names (VRP2.txt#1259).
    *   **VRP Pattern (MSI Repair EoP):** Standard user triggers MSI repair via `msiexec`, Windows Installer Service (SYSTEM) writes/executes temp files (`Google Update Setup`). Vulnerable if temp path logic (`GetTempFileName`) can be manipulated (e.g., by locking files) to force writes to user-controlled `%TEMP%` (VRP2.txt#139).
    *   **VRP Pattern (File ACLs):** Incorrectly setting permissive ACLs on files/directories created by elevated processes, allowing tampering by standard users (VRP2.txt#4151, #1191 - Google Update logs).

*   **Race Conditions (TOCTOU):** Time-of-check-to-time-of-use flaws, often involving file system operations.
    *   **VRP Pattern (File Ops Races):** Races between checking file properties (existence, type, size, reparse point status) and performing operations (write, delete, move, set permissions). Often exploitable using oplocks.
        *   *Examples:* Google Update logging race between `CreateFileW` and `IsReparsePoint` check (VRP2.txt#1191). MSI repair temp file locking race (VRP2.txt#139).
    *   **VRP Pattern (Update Logic Races - macOS):** Keystone race condition between checking the version inside an update package (`.tbz`) and verifying the code signature *after* extraction, allowing substitution of the package content mid-process (VRP2.txt#914).

*   **Insecure Linking Vulnerabilities:** Failure to securely handle symbolic links, junctions (Windows), or hardlinks (macOS) when performing privileged file operations.
    *   **VRP Pattern (Link Following):** Elevated process follows a planted link to operate on an unintended target file/directory. Robust checks (`FILE_FLAG_OPEN_REPARSE_POINT`, `O_NOFOLLOW`, checking target attributes) are needed *before* the operation.
        *   *Examples:* Google Update log rotation (`File::Move` without checks - VRP2.txt#1191), Crashpad temp file creation (VRP2.txt#9914), Installer logging (VRP2.txt#1333), Scoped temp dir deletion (VRP2.txt#1259), Uninstaller moving setup.exe (VRP2.txt#12876). Keystone lock file creation (macOS hardlink bypass - VRP2.txt#914). Keystone `.tbz` extraction (symlink inside archive - VRP2.txt#914).

*   **Insecure IPC/COM/XPC Handling:** Interfaces exposed by elevated services lacking proper authentication, authorization, or impersonation.
    *   **VRP Pattern (Windows COM Session Moniker):** `GoogleUpdate.ProcessLauncher` COM object callable by standard users allows launching commands in *other* user sessions via Session Monikers, potentially targeting an administrator session (VRP2.txt#3763).
    *   **VRP Pattern (Missing Impersonation - Windows COM):** Google Update `IAppBundleWeb->download` calls `DownloadManager::CachePackage` which uses `CallAsSelfAndImpersonate2` but subsequent file copy (`CopyFile`) might lack impersonation, allowing arbitrary file *read* by planting a junction in user's temp dir before the copy (VRP2.txt#1152).

*   **Insufficient Validation/Checks:** Other validation failures beyond linking or IPC.
    *   **VRP Pattern (Signature/Version Bypass):** Keystone skipping code signing verification if no previous version is installed or during race conditions (VRP2.txt#914). CRX3 signature bypass via embedded ZIP64 payload (relevant if installer uses CRX3 directly? VRP2.txt#3063).
    *   **VRP Pattern (Environment Variables):** Using user-controlled environment variables (like `%TEMP%`) in elevated processes when secure paths fail (VRP2.txt#139).

*   **Insecure Registry Operations (Windows):**
    *   **VRP Pattern (Registry Symlink Deletion):** Uninstaller deleting keys (e.g., `HKCU\Software\Microsoft\Active Setup\Installed Components\{Chrome-GUID}`) in user-writable locations. Attacker replaces key with a registry symlink pointing to a protected key (e.g., `HKCU\Software\Policies`). The elevated delete operation follows the link, deleting the *target* key. Attacker recreates target key with write permissions, allowing policy manipulation (e.g., `AlwaysInstallElevated` for further EoP) (VRP2.txt#8739).

## 3. Further Analysis and Potential Issues
*   **File System Operations & ACLs:** Deep dive into all privileged file operations (`CreateFileW`, `DeleteFileW`, `MoveFileExW`, `SetFileSecurityW`, directory creation/deletion, archive extraction).
    *   Where are files being created/deleted/moved? Focus on shared locations (`C:\Windows\Temp`, `C:\ProgramData`, `/tmp`, user profiles `%APPDATA%`).
    *   Are symlink/junction/hardlink checks performed *before* operations, and are they robust (checking target path attributes, not just the link itself)? `GetFileAttributesW` with `FILE_FLAG_OPEN_REPARSE_POINT`? `CreateFileW` with `FILE_FLAG_OPEN_REPARSE_POINT`? `O_NOFOLLOW` (macOS)?
    *   Are ACLs set securely on created directories/files to prevent tampering by lower-privileged users? (See VRP2.txt#4151, #1191).
    *   Can temporary filenames be predicted or monitored (`ReadDirectoryChangesW`)? (VRP2.txt#1259).
    *   Are oplocks considered in TOCTOU scenarios? (VRP2.txt#1191, #139).
*   **Log File Handling (`GoogleUpdate.ini`, `FileLogWriter`):** Although the `LogFilePath` issue (VRP2.txt#1363) is likely fixed, the entire logging mechanism (`FileLogWriter::CreateLoggingFile`, `FileLogWriter::ArchiveLoggingFile`) is sensitive due to running as SYSTEM and potentially writing to `C:\ProgramData\Google\Update\Log`. Review ACLs, file rotation logic (TOCTOU, VRP2.txt#1191), and handling of `MaxLogFileSize`. Re-check for reparse point vulnerabilities.
*   **IPC/COM/XPC Interfaces:** Identify all interfaces exposed by elevated services (Google Update, Keystone, Elevation Service). Audit methods for authorization checks (caller identity, permissions). Check for Session Moniker vulnerabilities (Windows, VRP2.txt#3763). Verify correct impersonation (`ImpersonateLoggedOnUser`, `RevertToSelf`, `CallAsSelfAndImpersonate2`) before performing actions, especially file operations (VRP2.txt#1152).
*   **Registry Operations (Windows):** Identify all registry keys modified/deleted by installer/uninstaller running with elevated privileges. If any key resides in a location modifiable by standard users (e.g., under HKCU), assess vulnerability to registry symlink attacks (VRP2.txt#8739).
*   **Update Package Handling (`.msi`, `.pkg`, `.tbz`):** Review archive extraction logic for path traversal and link vulnerabilities. Ensure signature/version verification happens *after* extraction to secure locations and *before* using the extracted files. Analyze race conditions between version checks and signature checks (VRP2.txt#914).
*   **Uninstallation Logic:** Review `uninstall.cc` and related platform-specific cleanup routines for insecure file/registry deletions in user-writable locations (VRP2.txt#8739, #12876).
*   **Crashpad Integration:** Analyze file operations performed by Crashpad setup/handler when invoked by the installer (VRP2.txt#9914).

## 4. Code Analysis
*   **Windows File Operations:**
    *   `chrome/installer/setup/setup_main.cc`: See `CreateTemporaryDirInDir`, deletion logic (e.g., for scoped dirs VRP2.txt#1259).
    *   `chrome/installer/util/logging_installer.cc`: Potentially writes `chrome_installer.log` / `chromium_installer.log` to `DIR_TEMP` (which is `C:\Windows\Temp` when run as SYSTEM) (VRP2.txt#1333). Uses `base::CreateDirectory` and `logging::InitLogging`.
    *   `base/files/file_util_win.cc`: Contains implementations for `CreateDirectory`, `DeleteFile`, `MoveFileEx`, `GetTempPathW`. Check flags used (e.g., `FILE_FLAG_OPEN_REPARSE_POINT`).
    *   `google_update/omaha/base/logging.cc` (`goopdate`): `FileLogWriter::CreateLoggingFile`, `FileLogWriter::ArchiveLoggingFile`. Check `CreateFileW` usage, ACL setting (`SetDacl`), `IsReparsePoint` checks, move logic. (VRP2.txt#1191, #4151).
    *   `google_update/omaha/goopdate/download_manager.cc` (`goopdate`): `CachePackage` uses `CallAsSelfAndImpersonate2` before `CopyFile` - check for missing impersonation leading to arbitrary file read (VRP2.txt#1152).
    *   `chrome/installer/setup/uninstall.cc`: `MoveSetupOutOfInstallFolder` copies `setup.exe` to temp dir (VRP2.txt#12876).
*   **macOS File Operations:**
    *   `ksinstall` (Keystone): Handles `.tbz` extraction (`/usr/bin/tar -Oxjf`). Needs check for symlinks within archive (VRP2.txt#914).
    *   Keystone framework: Lock file creation (`+[KSInstallSettings userInstallLockFile:]` writing to `/tmp/com.google.Keystone/.keystone_install_lock`). Check `open` flags vs hardlinks (VRP2.txt#914).
*   **Windows Registry Operations:**
    *   `chrome/installer/setup/uninstall.cc`: Deletes Active Setup keys (`DeleteRegistryKey`). Check interaction with registry symlinks (VRP2.txt#8739).
*   **Windows IPC/COM:**
    *   `google_update/google_update_idl.idl` (`goopdate`): Defines `IGoogleUpdate3Web`, `IAppBundleWeb`.
    *   `GoogleUpdate.ProcessLauncher` COM class (Check implementation): Exposes `IProcessLauncher` (`LaunchCmdLine`). Vulnerable to Session Moniker attack (VRP2.txt#3763).
    *   `google_update/omaha/goopdate/goopdate_coclass.cc` (implements `IAppBundleWeb`): Check `DownloadPackage` logic -> `DownloadManager::CachePackage` -> `CopyFile` regarding impersonation (VRP2.txt#1152).
    *   `chrome/elevation_service/`: Implements Chrome Elevation Service, check methods like `RunRecoveryCRXElevated` for file handling security.
*   **Signature/Version Checks:**
    *   Keystone (`ksinstall`): Check logic comparing versions before/after extraction and signature validation timing (VRP2.txt#914).
    *   `components/crx_file/crx_verifier.cc`: Verify CRX3 signatures. Check for potential bypasses (VRP2.txt#3063 - ZIP64 embed).

## 5. Areas Requiring Further Investigation
*   **Comprehensive File Op Audit:** Systematically audit all privileged file operations using the patterns above (temp dirs, logging, uninstallation, caching) for linking vulnerabilities and TOCTOU races. Pay close attention to `C:\Windows\Temp`, `C:\ProgramData`, `/tmp`.
*   **IPC/COM/XPC Interface Review:** Map out all exposed interfaces from elevated services. Verify authentication, authorization, and impersonation for each method, especially those performing file/registry operations or executing code.
*   **Registry Symlink Attack Surface:** Identify all registry keys modified by elevated installer/uninstaller components within user-writable hives (HKCU).
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