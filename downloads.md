# Component: Downloads

## 1. Component Focus
*   **Functionality:** Manages the downloading of files initiated by user actions, web content, or extensions. Includes determining file paths, handling MIME types, performing security checks (Safe Browsing, dangerous file types), displaying download UI (shelf, bubble, notifications), and interacting with the filesystem. Also covers the `chrome.downloads` extension API.
*   **Key Logic:** Download initiation (`DownloadManagerImpl`), target determination (`DownloadTargetDeterminer`), security checks (`DownloadProtectionService`, dangerous file type checks, `RenderFrameHostImpl::VerifyDownloadUrlParams`), UI management (`DownloadShelf`, `DownloadBubbleUIController`), extension API handling (`DownloadsApi`, including filename suggestion sanitization via `net::IsSafePortableRelativePath`). Information tracking via `DownloadItemImpl`. File operations via `BaseFile`.
*   **Core Files:**
    *   `content/public/browser/download_manager.h`, `content/browser/download/download_manager_impl.cc`
    *   `content/browser/renderer_host/render_frame_host_impl.cc` (contains `VerifyDownloadUrlParams`, `DownloadURL`)
    *   `components/download/internal/common/download_item_impl.cc`: Core download state representation.
    *   `components/download/internal/common/base_file.cc`, `base_file_win.cc`, `base_file_posix.cc`: Handles file IO, including the final rename via `MoveFileAndAdjustPermissions`.
    *   `components/download/internal/common/download_utils.cc`: Contains `download::DetermineLocalPath`.
    *   `net/base/filename_util.cc`, `net/base/filename_util_internal.h`: Contains `net::GenerateFileName`, `net::GenerateSafeFileName`, `net::IsSafePortableRelativePath` (declared in `.h`, implemented in `.cc`), `net::IsSafePortablePathComponent`.
    *   `chrome/browser/download/` (various files like `download_item_model.cc`, `download_target_determiner.cc`, `download_prefs.cc`, `download_commands.cc`)
    *   `chrome/browser/ui/views/download/` (UI views like `download_shelf_view.cc`, `download_bubble_view.cc`)
    *   `chrome/browser/extensions/api/downloads/downloads_api.cc`: Implements `chrome.downloads` API, including `DetermineFilenameFunction`, `ExtensionDownloadsEventRouter`, explicit `%` sanitization for `downloads.download` `filename` option, and validation using `net::IsSafePortableRelativePath`.
    *   `components/safe_browsing/content/browser/download/download_protection_service.h` ([safe_browsing_service.md](safe_browsing_service.md))

## 2. Potential Logic Flaws & VRP Relevance
*   **Download UI Spoofing/Obscuring:** Misleading users about the origin, filename, or status of a download.
    *   **VRP Pattern (Origin Spoofing):** Download UI (shelf, bubble, history) showing an incorrect origin. The `DownloadItemImpl` receives multiple potential origin indicators (the actual download URL chain, the referrer, the initiating tab's URL, the initiator origin). Ambiguity or incorrect logic in selecting which of these to display, especially in edge cases (redirects, `about:srcdoc`, blobs), can lead to spoofing. (VRP: `1157743`, `1281972`, `1499408`, `916831` - showed `about:srcdoc`; VRP2.txt#2294, #11062, #4900, #8323, #8581). History manipulation leading to fake download notifications (VRP: `1513412`, VRP2.txt#3449).
    *   **VRP Pattern (UI Obscuring/Interaction):** Download notifications obscuring fullscreen toast (VRP2.txt#10006, #10086). Keyjacking/Clickjacking the download bubble/shelf to force opening files (VRP2.txt#9286, #9287).
*   **File Type/Extension Handling & Path Validation:** Bypassing checks intended to warn about or block dangerous file types, or allowing downloads to unintended locations. This is often linked to insufficient sanitization of filenames, especially those suggested by extensions.
    *   **VRP Pattern (Type Masking):** Using double extensions, special characters (`%%`), or specific save dialog options (`showSaveFilePicker` accept types) to mask the true dangerous extension (e.g., saving `.lnk` or `.scf` as `.jpg`). (VRP: `1228653` - `.jpg.scf`, `1378895` - `%%` bypass, VRP2.txt#14019 - general masking, VRP2.txt#15866 - `%%` bypass via `Save As`). Abusing `.url` files (VRP: `1303486`, VRP2.txt#12993). **These likely rely on flaws in final path/extension validation within `DownloadTargetDeterminer`, as `net::GenerateSafeFileName` focuses mainly on extensions and reserved names.**
    *   **VRP Pattern (Dangerous Download Bypass):** Bypassing checks via DevTools protocol (`Page.downloadBehavior` VRP2.txt#16391) or potentially flaws in `DownloadTargetDeterminer` logic.
    *   **Validation at Initiation:** `RenderFrameHostImpl::VerifyDownloadUrlParams` specifically blocks downloads with `javascript:` URLs and checks for path traversal characters (`/`) in filenames suggested via context menu saves (`is_context_menu_save`). This prevents direct Javascript execution and restricts suggested save paths early in the process.
*   **Safe Browsing Bypass:** Circumventing Safe Browsing checks for malicious downloads.
    *   **VRP Pattern (Large Data URIs):** Using very large data URIs to cause the Safe Browsing ping to fail (e.g., exceed size limits), potentially skipping the check. (VRP: `1416794`, VRP2.txt#10091). Check interaction with `DownloadProtectionService`. See [safe_browsing_service.md](safe_browsing_service.md).
    *   **VRP Pattern (Referrer Chain):** Safe Browsing ping potentially failing if referrer chain (including data URIs) is too long (related to VRP2.txt#10091).
*   **Silent Downloads / Unintended File Writes:** Downloads occurring or overwriting files without adequate user confirmation or awareness.
    *   **VRP Pattern (Save Picker Interaction):** Using `showSaveFilePicker` combined with Enter key presses to achieve silent downloads/overwrites (VRP: `1243802`, VRP2.txt#9302). See [file_system_access.md](file_system_access.md).
    *   **VRP Pattern (Web Share API):** Using Web Share API with UNC paths (VRP2.txt#5730). See [webshare.md](webshare.md).
*   **Sandbox Bypass:** Downloads initiated from sandboxed contexts bypassing restrictions.
    *   **VRP Pattern (iframe `allow-downloads`):** Bypassing the lack of `allow-downloads` sandbox flag (VRP: `40060695`, VRP2.txt#11682).
    *   **VRP Pattern (iframe `noopener`):** Downloads initiated via `window.open` with `noopener` from sandboxed frames bypassing checks (VRP2.txt#1105). See [iframe_sandbox.md](iframe_sandbox.md).
*   **Extension API (`chrome.downloads`) Abuse:** Extensions using the Downloads API to perform unintended actions due to insufficient validation of extension-provided data.
    *   **VRP Pattern (Local File Read / Path Traversal via `onDeterminingFilename`):** Extensions with only `downloads` permission using `onDeterminingFilename` to suggest a filename containing path traversal components (`..`) or absolute paths. **The primary defense is `net::IsSafePortableRelativePath` (implemented in `filename_util.cc`) called within `ExtensionDownloadsEventRouter::DetermineFilename`. This function checks for `..`, absolute paths, separators within components, reserved names, etc.** Vulnerabilities likely relied on bypassing this check or exploiting weaknesses in downstream components like `BaseFile::MoveFileAndAdjustPermissions` and the OS file move APIs (e.g., Windows `IFileOperation`). (VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975). Interaction with FSA API (VRP: `1428743`).
    *   **VRP Pattern (Environment Variable Leak):** Using `chrome.downloads.download` with `saveAs: true` or `onDeterminingFilename` to suggest a name containing environment variables (`%VAR%`) to leak their values. **The `filename` option in `downloads.download` undergoes explicit `%` to `_` replacement in `DownloadsDownloadFunction::Run`. However, filenames suggested via `onDeterminingFilename` do not have this explicit sanitization.** The general filename validation (`net::IsSafePortableRelativePath`) does not check for `%`. The leak likely occurs if the path (especially from `onDeterminingFilename`) containing `%VAR%` is later passed to OS functions (e.g., via `BaseFile`) that perform expansion. (VRP2.txt#2935, #3189).

## 3. Further Analysis and Potential Issues
*   **Target Determination & File Operations:** Audit the entire flow from accepting an extension suggestion (`NotifyExtensionsDone`) through `DownloadTargetDeterminer` state changes to the final file move in `BaseFile::MoveFileAndAdjustPermissions`. **Verify that environment variables (`%VAR%`, especially from `onDeterminingFilename`) are robustly handled/rejected before the final file move OS API call.**
*   **Download UI Consistency & Origin Selection:** Ensure the displayed origin, filename, and security warnings are consistent and accurate across all UI surfaces (shelf, bubble, notifications, `chrome://downloads`). Crucially, determine *which field* (`url_chain` last element, `request_initiator`, `tab_url`, `referrer_url` passed to `DownloadItemImpl`) is used by each UI element to represent the download "source" and verify this logic handles redirects, service worker intercepts, extension-initiated downloads, blobs, and special schemes (like `about:srcdoc`) correctly. (Related to VRP: `1157743`, `916831`).
*   **Safe Browsing Integration:** Analyze the interaction between `DownloadManagerImpl` and `DownloadProtectionService`. Are checks always performed when expected? Can checks be bypassed by manipulating download parameters or timing? What happens if the check fails (VRP2.txt#10091 - resulted in no warning)?
*   **`chrome.downloads` API Security:** Path traversal via `onDeterminingFilename` is primarily mitigated by `net::IsSafePortableRelativePath`. Environment variable handling via `onDeterminingFilename` remains a potential concern depending on OS API behavior. (VRP2.txt#1176, #2935, etc.).
*   **Sandbox Interactions:** Verify that downloads initiated from sandboxed iframes correctly adhere to the `allow-downloads` flag in all cases (including `noopener` popups - VRP2.txt#1105).
*   **File System Interaction (FSA):** Analyze interactions between downloads and the File System Access API (VRP: `1428743`).

## 4. Code Analysis
*   `DownloadManagerImpl`: Core class managing downloads.
*   `DownloadTargetDeterminer`: Logic for determining the final download path. Contains the state machine (`DoGenerateTargetPath`, `DoDetermineLocalPath`, `DoDetermineIntermediatePath`, etc.). Relies on delegate and `net::GenerateFileName`.
*   `RenderFrameHostImpl::VerifyDownloadUrlParams`: Static function called during download initiation (e.g., in `RenderFrameHostImpl::DownloadURL`). Validates the download URL (blocking `javascript:` schemes) and checks for path traversal characters (`/`) in filenames suggested via context menu saves (`is_context_menu_save`).
*   `DownloadItemImpl` (`components/download/internal/common/download_item_impl.cc`): Represents download state.
*   `BaseFile` (`components/download/internal/common/base_file.cc`, `base_file_win.cc`, etc.): Handles file IO. `Rename` calls `MoveFileAndAdjustPermissions`.
*   `BaseFile::MoveFileAndAdjustPermissions` (`base_file_win.cc`): Performs the final file move using OS APIs (`IFileOperation` on Win). **Does not perform explicit path sanitization; relies on OS API behavior for filename validation (including rejecting `..`).**
*   `download::DetermineLocalPath` (`components/download/internal/common/download_utils.cc`): **Performs NO path validation/sanitization for non-Android cases.**
*   `net/base/filename_util.cc`: Contains `net::GenerateFileName`, `net::GenerateSafeFileName`, `net::IsReservedNameOnWindows`, **`net::IsSafePortableRelativePath`, `net::IsSafePortablePathComponent`**.
*   `net/base/filename_util_internal.h`: Declares internal helpers like `IsShellIntegratedExtension`.
*   `DownloadShelfView` / `DownloadBubbleViewController`: UI implementations.
*   `DownloadsApi::DetermineFilenameFunction` / `ExtensionDownloadsEventRouter::DetermineFilename` (`chrome/browser/extensions/api/downloads/downloads_api.cc`): Handles `onDeterminingFilename` listener. Calls `net::IsSafePortableRelativePath` for path traversal sanitization.
*   `DownloadsApi::DownloadFunction` (`chrome/browser/extensions/api/downloads/downloads_api.cc`): Handles `chrome.downloads.download`. **Explicitly replaces `%` with `_` in the `filename` option.**
*   `DownloadProtectionService`: Interface with Safe Browsing.
*   `content/browser/download/download_utils.cc`: Utility functions.

## 5. Areas Requiring Further Investigation
*   **Environment Variable Handling:** How are environment variables (`%VAR%` in filenames, especially from `onDeterminingFilename`) handled by `BaseFile::MoveFileAndAdjustPermissions` and the underlying OS APIs (e.g., `IFileOperation`)? Does expansion occur?
*   **UI Origin Display Logic:** Trace which specific field from `DownloadItemImpl`/`DownloadRequestInfo` is used by each UI component (shelf, bubble, history) and how edge cases are handled (VRP: `916831`, `1157743`).
*   **Safe Browsing Failure Modes:** What happens if the `DownloadProtectionService` check fails? Ensure safe default behavior (VRP2.txt#10091).
*   **Download Bubble Interactions:** Test the new download bubble UI for clickjacking/keyjacking vulnerabilities (VRP2.txt#9287).

## 6. Related VRP Reports
*   **UI Origin Spoofing:** VRP: `1157743`, `1281972`, `1499408`, `916831` (srcdoc); VRP2.txt#2294, #11062, #4900, #8323, #8581
*   **UI Interaction:** VRP2.txt#9286 (Keyjacking), #9287 (Clickjacking), #10006, #10086 (Obscuring fullscreen toast)
*   **File Type Masking:** VRP: `1228653` (`.jpg.scf`), `1378895` (`%%` bypass), `1303486` (`.url` files); VRP2.txt#14019 (General), #15866 (`%%` bypass), #12993 (`.url` files)
*   **Dangerous Download Bypass:** VRP2.txt#16391 (DevTools `Page.downloadBehavior`)
*   **Safe Browsing Bypass:** VRP: `1416794`; VRP2.txt#10091 (Large data URI)
*   **Silent Download/Write:** VRP: `1243802` (Save Picker + Enter); VRP2.txt#9302 (Save Picker + Enter), #5730 (Web Share UNC)
*   **Sandbox Bypass:** VRP: `40060695` (`allow-downloads`); VRP2.txt#11682 (`allow-downloads`), #1105 (`noopener`)
*   **Extension API (`chrome.downloads`):**
    *   *Local File Read / Path Traversal:* VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975 (Via `onDeterminingFilename`. Mitigated primarily by `net::IsSafePortableRelativePath`. Vulnerabilities likely involved bypassing this or exploiting downstream OS API behavior.)
    *   *FSA Interaction:* VRP: `1428743`
    *   *Env Var Leak:* VRP2.txt#2935 (`download` option - mitigated by `%` replacement), #3189 (`onDeterminingFilename` - `%` not explicitly replaced, relies on OS API behavior).
    *   *SOP Bypass:* VRP2.txt#11328
*   **History Spoofing:** VRP: `1513412`; VRP2.txt#3449

*(See also [safe_browsing_service.md](safe_browsing_service.md), [extensions_api.md](extensions_api.md), [file_system_access.md](file_system_access.md), [iframe_sandbox.md](iframe_sandbox.md))*
