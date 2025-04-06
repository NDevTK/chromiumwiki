# Component: Installer &amp; Updater (Windows/macOS)

## 1. Component Focus
*   Focuses on the security of the Chrome/Chromium installer (`setup.exe`, `.msi`, `.pkg`) and the update services (Google Update/Omaha on Windows, Keystone on macOS).
*   Handles installation, updates, and uninstallation, often running with elevated privileges (SYSTEM on Windows, root on macOS).
*   Involves file operations (copying, deleting, creating directories, setting ACLs), registry operations (Windows), IPC/COM communication (Windows), and potentially XPC communication (macOS).
*   Relevant components/executables: `setup.exe`, `GoogleUpdate.exe`, `goopdate.dll`, Chrome Elevation Service (Windows), Keystone framework (`ksinstall`), Crashpad handler/installer actions.

## 2. Potential Logic Flaws &amp; VRP Relevance
*   **Arbitrary File Operations (High EoP Risk):** Vulnerabilities leading to arbitrary file creation, deletion, or modification due to insecure handling of temporary directories, log files, update packages, or scoped directories during installation/uninstallation. Often exploitable via junctions, symlinks, or hardlinks planted by low-privilege users in predictable/writable locations.
    *   **VRP Pattern (Temp/Log Dirs):** Creating/deleting files/dirs in world-writable locations like `C:\Windows\Temp\` (VRP2.txt#1259 - scoped_dir deletion, #1333 - installer log), `%APPDATA%\Local` (VRP2.txt#4151 - logging), `C:\ProgramData\Google\Update\Log` (VRP2.txt#1191, #4151 - logging), `/tmp/com.google.Keystone` (VRP2.txt#914 - lock file) without proper symlink/hardlink checks or secure ACLs. Using predictable names or names discoverable via `ReadDirectoryChangesW` (VRP2.txt#1259). Crashpad creating files in temp (VRP2.txt#9914). Uninstaller moving files to temp (VRP2.txt#12876).
    *   **VRP Pattern (MSI Repair):** Windows Installer Service (SYSTEM) performing repair operations triggered by standard user, leading to insecure file writes if temporary paths can be manipulated (VRP2.txt#139).
*   **Race Conditions (TOCTOU):**
    *   **VRP Pattern (File Ops):** Races between checking file properties (existence, type, size) and performing operations like writing, deleting, or setting permissions, especially with log files or update caches. Exploitable using oplocks. (VRP2.txt#1191 - log rotation, #139 - MSI repair).
    *   **VRP Pattern (Update Logic - macOS):** Race between checking installed version vs. checking codesign of the update package (`.tbz`), allowing substitution of the package between checks. (VRP2.txt#914 - Keystone).
*   **Insecure IPC/COM Handling (Windows):**
    *   **VRP Pattern (Session Moniker):** COM objects exposed by the update service (`GoogleUpdate.ProcessLauncher`) callable by low-privilege users via Session Monikers, allowing command execution in another (potentially elevated) user session. (VRP2.txt#3763).
*   **Insufficient Validation/Checks:**
    *   **VRP Pattern (Linking):** Lack of robust symlink/hardlink/junction checks before performing privileged file operations (deletion, creation, ACL setting). (VRP2.txt#914 - Keystone `.tbz` extraction, #1259 - scoped dir deletion, #1191 - logging, #8739 - registry symlinks). `O_NOFOLLOW` ineffective against hardlinks on macOS (VRP2.txt#914). Checking `IsReparsePoint` after `CreateFileW` can be too late if raced (VRP2.txt#1191).
    *   **VRP Pattern (Signature/Version):** Missing or bypassable code signature checks, especially when no previous version is installed or during race conditions (VRP2.txt#914 - Keystone).
    *   **VRP Pattern (Environment Variables):** Improper handling of environment variables like `%TEMP%`, leading to writes in user-controlled locations when expected paths are inaccessible (VRP2.txt#139).
    *   **VRP Pattern (Permissions):** Incorrect permission inheritance or handling, e.g., related to `AlwaysInstallElevated` policy (VRP2.txt#8739).
*   **Arbitrary File Read:**
    *   **VRP Pattern (Missing Impersonation):** COM interfaces (`IAppBundleWeb->download`) calling internal functions (`CachePackage` via `CallAsSelfAndImpersonate2`) that perform file copying without proper impersonation, allowing reads from attacker-controlled paths via junctions/links. (VRP2.txt#1152).
*   **Insecure Registry Operations (Windows):**
    *   **VRP Pattern (Symlink Deletion):** Deleting registry keys (e.g., uninstaller cleaning up `HKCU\Software\Microsoft\Active Setup\Installed Components\{GUID}`) located in user-writable areas allows replacing the key with a registry symlink. The deletion operation then targets the *linked* key (e.g., `HKCU\Software\Policies`), allowing the attacker to recreate it with write permissions. (VRP2.txt#8739).

## 3. Further Analysis and Potential Issues
*   **File System Operations & ACLs:** Deep dive into all privileged file operations (`CreateFileW`, `DeleteFileW`, `MoveFileExW`, `SetFileSecurityW`, directory creation/deletion, archive extraction).
    *   Where are files being created/deleted/moved? Focus on shared locations (`C:\Windows\Temp`, `C:\ProgramData`, `/tmp`, user profiles `%APPDATA%`).
    *   Are symlink/junction/hardlink checks performed *before* operations, and are they robust (checking target path attributes, not just the link itself)? `GetFileAttributesW` with `FILE_FLAG_OPEN_REPARSE_POINT`? `CreateFileW` with `FILE_FLAG_OPEN_REPARSE_POINT`? `O_NOFOLLOW` (macOS)?
    *   Are ACLs set securely on created directories/files to prevent tampering by lower-privileged users? (See VRP2.txt#4151, #1191).
    *   Can temporary filenames be predicted or monitored (`ReadDirectoryChangesW`)? (VRP2.txt#1259).
    *   Are oplocks considered in TOCTOU scenarios? (VRP2.txt#1191, #139).
*   **Log File Handling (`GoogleUpdate.ini`, `FileLogWriter`):** Although the `LogFilePath` issue (VRP2.txt#1363) is likely fixed, the entire logging mechanism (`FileLogWriter::CreateLoggingFile`, `ArchiveLoggingFile`) is sensitive due to running as SYSTEM and potentially writing to `C:\ProgramData\Google\Update\Log`. Review ACLs, file rotation logic (TOCTOU, VRP2.txt#1191), and handling of `MaxLogFileSize`.
*   **IPC/COM/XPC Interfaces:** Identify all interfaces exposed by elevated services (Google Update, Keystone, Elevation Service). Audit methods for authorization checks (caller identity, permissions). Check for Session Moniker vulnerabilities (Windows, VRP2.txt#3763). Verify correct impersonation (`ImpersonateLoggedOnUser`, `RevertToSelf`, `CallAsSelfAndImpersonate2`) before performing actions, especially file operations (VRP2.txt#1152).
*   **Registry Operations (Windows):** Identify all registry keys modified/deleted by installer/uninstaller running with elevated privileges. If any key resides in a location modifiable by standard users (e.g., under HKCU), assess vulnerability to registry symlink attacks (VRP2.txt#8739).
*   **Update Package Handling (`.msi`, `.pkg`, `.tbz`):** Review archive extraction logic for path traversal and link vulnerabilities. Ensure signature/version verification happens *after* extraction to secure locations and *before* using the extracted files. Analyze race conditions between version checks and signature checks (VRP2.txt#914).
*   **Uninstallation Logic:** Review `uninstall.cc` and related platform-specific cleanup routines for insecure file/registry deletions in user-writable locations (VRP2.txt#8739, #12876).
*   **Crashpad Integration:** Analyze file operations performed by Crashpad setup/handler when invoked by the installer (VRP2.txt#1328).

## 4. Code Analysis
*   **Windows File Operations:**
    *   `chrome/installer/setup/setup_main.cc`: See `CreateTemporaryDirInDir`, deletion logic (e.g., for scoped dirs VRP2.txt#1259).
    *   `chrome/installer/util/logging_installer.cc`: Potentially writes `chrome_installer.log` / `chromium_installer.log` to `DIR_TEMP` (which is `C:\Windows\Temp` when run as SYSTEM) (VRP2.txt#1333). Uses `base::CreateDirectory` and `logging::InitLogging`.
    *   `base/files/file_util_win.cc`: Contains implementations for `CreateDirectory`, `DeleteFile`, `MoveFileEx`, `GetTempPathW`. Check flags used (e.g., `FILE_FLAG_OPEN_REPARSE_POINT`).
    *   `google_update/omaha/base/logging.cc` (`goopdate`): `FileLogWriter::CreateLoggingFile`, `FileLogWriter::ArchiveLoggingFile`. Check `CreateFileW` usage, ACL setting (`SetDacl`), `IsReparsePoint` checks, move logic. (VRP2.txt#1191, #4151).
    *   `google_update/omaha/goopdate/download_manager.cc` (`goopdate`): `CachePackage` uses `CallAsSelfAndImpersonate2` before `CopyFile` - check for missing impersonation leading to arbitrary file read (VRP2.txt#1152).
*   **macOS File Operations:**
    *   `ksinstall` (Keystone): Handles `.tbz` extraction (`/usr/bin/tar -Oxjf`). Needs check for symlinks within archive (VRP2.txt#914).
    *   Keystone framework: Lock file creation (`+[KSInstallSettings userInstallLockFile:]` writing to `/tmp/com.google.Keystone/.keystone_install_lock`). Check `open` flags vs hardlinks (VRP2.txt#914).
*   **Windows Registry Operations:**
    *   `chrome/installer/setup/uninstall.cc`: Deletes Active Setup keys (`DeleteRegistryKey`). Check interaction with registry symlinks (VRP2.txt#8739).
*   **Windows IPC/COM:**
    *   `google_update/google_update_idl.idl` (`goopdate`): Defines `IGoogleUpdate3Web`, `IAppBundleWeb`.
    *   `GoogleUpdate.ProcessLauncher` COM class (Check implementation): Exposes `IProcessLauncher` (`LaunchCmdLine`). Vulnerable to Session Moniker attack (VRP2.txt#3763).
    *   `chrome/elevation_service/`: Implements Chrome Elevation Service, check methods like `RunRecoveryCRXElevated` for file handling security.
*   **Signature/Version Checks:**
    *   Keystone (`ksinstall`): Check logic comparing versions before/after extraction and signature validation timing (VRP2.txt#914).

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
*   VRP2.txt#1328: Crashpad Temp File Creation EoP (Symlink)
*   VRP2.txt#1363: Google Update Logging EoP (`GoogleUpdate.ini` `LogFilePath` - likely fixed)
*   VRP2.txt#4151: Google Update Logging EoP (Permissive ACLs on log dir + junction)
*   VRP2.txt#8739: Uninstaller Registry EoP (Registry symlink deletion)
*   VRP2.txt#1333: Installer Logging EoP (`chrome_installer.log` in `C:\Windows\Temp` + symlink)
*   VRP2.txt#12876: Uninstaller Temp File EoP (Move setup to temp + symlink)
*   VRP2.txt#1152: Google Update Arbitrary File Read (`IAppBundleWeb->download` cache + missing impersonation)
*   VRP2.txt#3763: Google Update COM EoP (`GoogleUpdate.ProcessLauncher` Session Moniker)

*(List reviewed against VRP2.txt for Installer/Updater EoP reports >$1000).*