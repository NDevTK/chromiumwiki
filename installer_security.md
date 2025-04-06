# Component: Installer &amp; Updater (Windows/macOS)

## 1. Component Focus
*   Focuses on the security of the Chrome/Chromium installer and the Google Update service (Windows) / Keystone (macOS).
*   Handles installation, updates, and uninstallation, often running with elevated privileges (SYSTEM on Windows, root on macOS).
*   Involves file operations (copying, deleting, creating directories), registry operations (Windows), IPC/COM communication (Windows), and potentially XPC communication (macOS).
*   Relevant components/executables: `setup.exe`, `GoogleUpdate.exe`, `goopdate.dll`, `Keystone` framework, `ksinstall`.

## 2. Potential Logic Flaws &amp; VRP Relevance
*   **Arbitrary File Operations:** Vulnerabilities leading to arbitrary file creation, deletion, or modification due to insecure handling of temporary directories (e.g., `C:\Windows\Temp\`, `%APPDATA%\Local`), log files (`C:\GoogleUpdate.ini`, `C:\ProgramData\Google\Update\Log`), update packages, or scoped directories during installation/uninstallation. Often exploitable via junctions, symlinks, or hardlinks. (VRP2.txt#139 - MSI repair EoP, VRP2.txt#1191 - EoP via GoogleUpdate.ini logging bypass, VRP2.txt#1259 - EoP via scoped dir deletion, VRP2.txt#1328 - Crashpad arbitrary file create, VRP2.txt#4151 - GoogleUpdate logging arbitrary file creation, VRP2.txt#8739 - EoP via registry symlink + uninstaller, VRP2.txt#1333 - Installer log arbitrary file write, VRP2.txt#12876 - Uninstaller arbitrary file write/execute). Note: VRP2.txt#1363 appears duplicate or related to #1191/#4151.
*   **Race Conditions:** Issues during update processes, such as Time-of-check-to-time-of-use (TOCTOU) between checking component versions and verifying code signatures after decompression, potentially allowing malicious package substitution. (VRP2.txt#914 - Keystone race condition). Also, TOCTOU during log file rotation (VRP2.txt#1191). Oplocks can be used to win races against file operations (VRP2.txt#139, VRP2.txt#1191).
*   **Insecure IPC/COM Handling (Windows):** COM objects exposed by the update service (like `GoogleUpdate.ProcessLauncher`) might be callable by low-privilege users through techniques like Session Monikers, potentially bypassing security checks or allowing actions in other user sessions. (VRP2.txt#3763 - Session Moniker EoP).
*   **Insufficient Validation/Checks:** Missing or incomplete code signature checks in certain update scenarios (VRP2.txt#914 - Keystone install without prior version). Lack of symlink/hardlink checks during decompression or file operations (VRP2.txt#914 - Keystone symlink in tbz, VRP2.txt#8739 - Registry symlinks). Improper handling of environment variables (`%TEMP%`) leading to writes in user-controlled locations (VRP2.txt#139). Incorrect permission checks or inheritance (VRP2.txt#8739 - `AlwaysInstallElevated`).
*   **Arbitrary File Read:** Vulnerabilities in exposed interfaces allowing reading of arbitrary files, for instance, during package caching without proper impersonation (VRP2.txt#1152 - Google Update `IAppBundleWeb->download`).

## 3. Further Analysis and Potential Issues
*   **File System ACLs:** The installer/updater frequently interacts with directories like `C:\Windows\Temp`, `C:\ProgramData\Google\Update`, `%APPDATA%\Local`, and `/tmp/`. The default permissions on these directories, and the permissions applied to files/subdirectories created by the updater, are critical. Permissive ACLs (e.g., allowing standard users write/delete access or attribute modification) can enable linking/junction attacks (VRP2.txt#139, #4151, #1191, #914, #1333). Monitoring tools like `ReadDirectoryChangesW` can predict temporary file/directory names (VRP2.txt#1259). Locking files (e.g., with `FILE_SHARE_READ`) can manipulate installer behavior (VRP2.txt#139).
*   **Log File Handling:** `GoogleUpdate.ini` (VRP2.txt#1363, #1191, #4151) historically allowed specifying arbitrary log paths. While fixed to use `C:\ProgramData\Google\Update\Log`, the handling of log file creation (`CreateFileW`), size limits (`MaxLogFileSize`), rotation (`FileLogWriter::ArchiveLoggingFile`), and ACL setting remains sensitive. TOCTOU vulnerabilities during these operations, especially when combined with linking attacks, are a recurring pattern.
*   **Temporary File/Directory Operations:** Installers often create temporary directories (e.g., `C:\Windows\Temp\scoped_dirXXXX_XXXXXXXXX`, `/tmp/ksinstall.XXXXXXXXXX`). Deletion of these directories (`DeleteFileW`, recursive deletion) can be exploited if junctions are planted inside before deletion (VRP2.txt#1259). Similarly, temporary extraction paths for checking code signatures (`/tmp/{random_path}/GoogleSoftwareUpdate.bundle` in VRP2.txt#914) are targets.
*   **IPC/COM/XPC Interfaces:** Interfaces like `GoogleUpdate.ProcessLauncher` (VRP2.txt#3763) and `IAppBundleWeb` (VRP2.txt#1152) need careful auditing to ensure proper authorization checks are performed, considering techniques like Session Monikers on Windows. Are callers properly identified and restricted? Does the service impersonate correctly when performing file operations on behalf of a client? (`CallAsSelfAndImpersonate2` used in VRP2.txt#1152).
*   **Registry Operations (Windows):** Uninstallation routines might delete specific registry keys (e.g., Chrome's GUID under `HKCU\Software\Microsoft\Active Setup\Installed Components`). If these keys are user-writable, they can be replaced with registry symlinks pointing to protected keys (like `HKCU\Software\Policies`). Windows API behavior for deleting registry links can lead to the **target** key being deleted, allowing an attacker to recreate the target with user-writable permissions (VRP2.txt#8739).
*   **Update Package Handling:** Decompressing `.tbz` archives (macOS Keystone - VRP2.txt#914) or handling MSI packages (Windows - VRP2.txt#139) requires careful handling of paths within the archive to prevent directory traversal or symlink/hardlink attacks that could lead to arbitrary file writes during extraction or installation. Code signature verification must be robust and occur *after* potentially malicious links could be resolved.

## 4. Code Analysis
*   **Windows File Operations:**
    *   `setup_main.cc` [CreateTemporaryDirInDir](https://source.chromium.org/chromium/chromium/src/+/main:chrome/installer/setup/setup_main.cc;l=926), [DeleteFile](https://source.chromium.org/chromium/chromium/src/+/main:chrome/installer/setup/setup_main.cc;l=944) - Handling of scoped temporary directories. (See VRP2.txt#1259)
    *   `goopdate/logging.cc` ([FileLogWriter::CreateLoggingFile](https://github.com/google/omaha/blob/master/omaha/base/logging.cc#L1075), [FileLogWriter::ArchiveLoggingFile](https://github.com/google/omaha/blob/master/omaha/base/logging.cc#L1154)) - Log file creation, ACL setting, rotation. Check for `IsReparsePoint` after `CreateFileW` attempts. (See VRP2.txt#1191, #4151, #1363, #1333)
    *   `goopdate/download_manager.cc` ([DoDownloadPackage](https://chromium.googlesource.com/external/omaha/+/e2c3f15816f1a394e56433de4fb58db30548fdb0/goopdate/download_manager.cc#325), `CallAsSelfAndImpersonate2`, `CachePackage`) - Caching downloaded packages, potential for missing impersonation. (See VRP2.txt#1152)
*   **macOS File Operations:**
    *   `ksinstall` - Handles installation from `.tbz`. Check decompression logic (`/usr/bin/tar -Oxjf`), symlink handling during extraction and code signing checks. (See VRP2.txt#914)
    *   Keystone framework - Lock file creation (`+[KSInstallSettings userInstallLockFile:]` writing to `/tmp/com.google.Keystone/.keystone_install_lock`), use of `O_NOFOLLOW` vs. hard links. (See VRP2.txt#914)
*   **Windows Registry Operations:**
    *   `uninstall.cc` [DeleteRegistryKey](https://source.chromium.org/chromium/chromium/src/+/main:chrome/installer/setup/uninstall.cc) - Deletion of Active Setup keys. Check if target location is user-writable and susceptible to registry symlink attacks. (See VRP2.txt#8739)
*   **Windows IPC/COM:**
    *   `google_update_idl.idl` ([IGoogleUpdate3Web](https://chromium.googlesource.com/chromium/src/+/32352ad08ee673a4d43e8593ce988b224f6482d3/google_update/google_update_idl.idl;l=6488), [IAppBundleWeb](https://chromium.googlesource.com/chromium/src/+/32352ad08ee673a4d43e8593ce988b224f6482d3/google_update/google_update_idl.idl;l=6498)) - Exposed interfaces.
    *   `GoogleUpdate.ProcessLauncher` - COM Class exposing `IProcessLauncher` interface. Check `LaunchCmdLine` for security checks, especially when invoked via Session Moniker. (See VRP2.txt#3763)

## 5. Areas Requiring Further Investigation
*   Thorough review of all file system and registry operations performed with elevated privileges, focusing on TOCTOU vulnerabilities and correct ACL/permission handling, especially in temporary or shared locations.
*   Security analysis of all exposed IPC/COM/XPC interfaces, verifying caller identity, permissions, and proper impersonation during subsequent actions.
*   Audit of signature verification logic in relation to update package extraction, ensuring links are handled safely and verification happens on the actual content to be installed.
*   Review of uninstallation procedures for safe handling of file and registry key deletion, particularly in user-writable locations.
*   Analysis of Crashpad installer actions for similar file operation vulnerabilities (VRP2.txt#1328).

## 6. Related VRP Reports
*   VRP2.txt#139: Security: Chrome Enterprise MSI installer Elevation of Privileges Vulnerability (MSI repair, temp file locking, `%TEMP%` abuse)
*   VRP2.txt#914: Security: Elevation of Privilege via Vulnerability in Keystone for macOS (Lock file hardlink, codesign bypass race condition, symlink in tbz)
*   VRP2.txt#1191: Security: Incomplete fix for CVE-2021-30577 (Google Update logging EoP, TOCTOU, oplock)
*   VRP2.txt#1259: Security: Elevation of Privileges in chrome installer when removing scoped directory during updates (Temp dir deletion + junction)
*   VRP2.txt#1328: Security: Chrome Crashpad arbitrary file create (Temp file creation + symlink)
*   VRP2.txt#1363: Security: Local Elevation of Privilege vulnerability in Google Update Service (GoogleUpdate.ini LogFilePath - Note: Original report, likely fixed/superseded by #1191/#4151)
*   VRP2.txt#4151: Security: Google Update for Windows allows arbitrary file creation when logs are enabled (Permissive ACLs on log dir + junction)
*   VRP2.txt#8739: Inadequate Registry management within the Chrome uninstaller resulting in privilege escalation (Registry key deletion + symlink)
*   VRP2.txt#1333: Chromium arbitrary file create/write vulnerability (Installer log file creation + symlink)
*   VRP2.txt#12876: Chromium arbitrary file create/write and execute vulnerability (Uninstaller temp file move + symlink)
*   VRP2.txt#1152: Security: Arbitrary file read when caching file using CallAsSelfAndImpersonate2 (Google Update `IAppBundleWeb->download` cache)

*(This list has been reviewed against VRP2.txt and seems comprehensive for >$1000 EoP reports related to Installer/Updater at the time of writing).*