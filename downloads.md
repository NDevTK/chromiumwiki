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
    *   `net/base/mime_util.cc`: Contains MIME type mapping (`GetMimeTypeFromFile`), relies on extension.
    *   `net/base/filename_util.cc`, `net/base/filename_util_internal.h`: Contains `net::GenerateFileName`, `net::GenerateSafeFileName`, `net::IsSafePortableRelativePath` (declared in `.h`, implemented in `.cc`), `net::IsSafePortablePathComponent`.
    *   `chrome/browser/download/` (various files like `download_item_model.h/cc`, `download_target_determiner.cc`, `download_prefs.cc`, `download_commands.cc`, `download_ui_model.h/cc`, `download_shelf_context_menu.cc`)
    *   `chrome/browser/ui/views/download/` (UI views like `download_shelf_view.cc`, `download_bubble_row_view.h/cc`, `download_shelf_context_menu_view.cc`)
    *   `chrome/browser/download/notification/` (Notification UI like `download_item_notification.h/cc`, `download_notification_manager.h/cc`)
    *   `chrome/browser/ui/webui/downloads/` (WebUI like `downloads_ui.cc`, `downloads_list_tracker.cc`, `downloads_dom_handler.cc`)
    *   `chrome/browser/resources/downloads/`: Frontend resources for `chrome://downloads`.
    *   `chrome/browser/extensions/api/downloads/downloads_api.cc`: Implements `chrome.downloads` API, including `DetermineFilenameFunction`, `ExtensionDownloadsEventRouter`, explicit `%` sanitization for `downloads.download` `filename` option, and validation using `net::IsSafePortableRelativePath`.
    *   `components/safe_browsing/content/browser/download/download_protection_service.h` ([safe_browsing_service.md](safe_browsing_service.md))
    *   `components/safe_browsing/content/resources/download_file_types.asciipb`: Configuration for dangerous file types.
    *   `components/url_formatter/`: Contains `url_formatter.cc`, `elide_url.cc` used for displaying URLs securely.
    *   `ui/shell_dialogs/`: Contains file dialog implementations (`select_file_dialog.h`, `select_file_dialog_win.cc`, `execute_select_file_win.cc`).
    *   `chrome/browser/platform_util.cc`, `platform_util_win.cc`: Cross-platform and Windows-specific file opening logic.
    *   `ui/base/win/shell.cc`: Windows shell integration helpers (`OpenFileViaShell` uses `ShellExecuteExW`).

## 2. Potential Logic Flaws & VRP Relevance
*   **Download UI Spoofing/Obscuring:** Misleading users about the origin, filename, or status of a download. (See README Tip #1 - UI Security)
    *   **Origin Display Summary (Desktop):**
        *   **Shelf/Bubble Rows:** Show *filename* + status text. Status text does *not* include origin URL (`DownloadItemModel::ShouldPromoteOrigin` defaults to false).
        *   **`chrome://downloads` Page:** Shows *filename* + status text + clickable link using the **Final URL**'s origin (`DownloadListTracker` uses `DownloadItem::GetURL()` formatted by `FormatUrlForSecurityDisplay`). `FormatUrlForSecurityDisplay` falls back to `FormatUrl` for non-standard schemes, which returns the literal scheme and path (e.g., "about:srcdoc").
        *   **Notifications:** Show *filename* + status text + **hostname of Final URL** (`DownloadItemNotification` uses `DownloadItem::GetURL()` formatted by `FormatUrlForSecurityDisplay`, omitting scheme).
        *   **Context Menu:** Shows standard commands; does *not* display origin/URL in menu items.
    *   **VRP Pattern (Origin Spoofing):** VRPs reporting origin spoofing likely relate to: (See README Tip #1 - Dialog/Prompt Spoofing)
        *   **Edge Cases:** Specific redirect scenarios (VRP2.txt#2294, #12168) or non-standard schemes (`about:srcdoc` VRP: `916831`, `11062`, VRP2.txt#11062) where the displayed final URL/hostname might still be confusing or misleading (e.g., `FormatUrl` returns "about:srcdoc").
        *   **Android Differences:** Android notifications *can* display the Original URL based on a flag (see `DownloadNotificationFactory.java`).
        *   **Long Hostnames/Subdomains:** Obscuring the true domain by using long subdomains in the final URL (VRP2.txt#8323, #8581 - Android focused). (See README Tip #1 - Address Bar Spoofing)
        *   **History Manipulation:** Tricking the browser into showing a download notification seemingly from a previous, legitimate site (VRP: `1513412`; VRP2.txt#3449).
    *   **VRP Pattern (UI Obscuring/Interaction):** Download notifications obscuring fullscreen toast (VRP2.txt#10006, #10086). Keyjacking/Clickjacking the download bubble/shelf to force opening files (VRP2.txt#9286, #9287). (See README Tip #1 - Input/Interaction Hijacking)
*   **File Type/Extension Handling & Path Validation:** Bypassing checks intended to warn about or block dangerous file types, or allowing downloads to unintended locations. This is often linked to insufficient sanitization of filenames, especially those suggested by extensions or manipulated in file dialogs. (See README Tip #6 - Filesystem & Scheme Handling)
    *   **VRP Pattern (Type Masking):** Using double extensions, special characters (`%%`), or specific save dialog options (`showSaveFilePicker` accept types) to mask the true dangerous extension (e.g., saving `.lnk` or `.scf` as `.jpg`). `net::GetMimeTypeFromFile` relies on the *last* extension, offering limited protection against this. `DownloadTargetDeterminer` applies limited pre-dialog sanitization on Windows (e.g., removing `%`). However, the final filename often depends on the native "Save As" dialog (`IFileSaveDialog`/`GetSaveFileName` on Windows), which may prioritize user input over suggested types/extensions, allowing masking to succeed. (VRP: `1228653` - `.jpg.scf`, `1378895` - `%%` bypass, VRP2.txt#14019 - general masking, VRP2.txt#15866 - `%%` bypass via `Save As`). Abusing `.url` files (VRP: `1303486`, VRP2.txt#12993).
    *   **VRP Pattern (Dangerous Download Bypass):** Bypassing checks via DevTools protocol (`Page.downloadBehavior` VRP2.txt#16391) or potentially flaws in `DownloadTargetDeterminer` logic.
    *   **VRP Pattern (OS Handler Abuse / Sandbox Bypass):** Files like `.SettingContent-ms` are recognized as dangerous by Safe Browsing policies (`download_file_types.asciipb`), but if the user bypasses the warning, `platform_util::OpenItem` calls `ui::win::OpenFileViaShell` -> `ShellExecuteExW` which executes the file's embedded command via the OS handler outside the sandbox. (VRP `40060695`).
    *   **Validation at Initiation:** `RenderFrameHostImpl::VerifyDownloadUrlParams` specifically blocks downloads with `javascript:` URLs and checks for path traversal characters (`/`) in filenames suggested via context menu saves (`is_context_menu_save`). This prevents direct Javascript execution and restricts suggested save paths early in the process.
*   **Safe Browsing Bypass:** Circumventing Safe Browsing checks for malicious downloads.
    *   **VRP Pattern (Large Request Payload):** Using very long URL or referrer chains can exceed server limits, causing the SB ping to fail (e.g., HTTP 413). `CheckClientDownloadRequestBase` interprets this as `DownloadCheckResult::UNKNOWN`, which `ChromeDownloadManagerDelegate` then treats as `SAFE` unless the file extension is inherently dangerous. (VRP: `1416794`, VRP2.txt#10091, #10114). See [safe_browsing_service.md](safe_browsing_service.md).
*   **Silent Downloads / Unintended File Writes:** Downloads occurring or overwriting files without adequate user confirmation or awareness.
    *   **VRP Pattern (Save Picker Interaction):** Using `showSaveFilePicker` combined with Enter key presses to achieve silent downloads/overwrites (VRP: `1243802`, VRP2.txt#9302). See [file_system_access.md](file_system_access.md). (See README Tip #6)
    *   **VRP Pattern (Web Share API):** Using Web Share API with UNC paths (VRP2.txt#5730). See [webshare.md](webshare.md). (See README Tip #6)
*   **Sandbox Bypass:** Downloads initiated from sandboxed contexts bypassing restrictions, or downloaded files leading to execution outside the sandbox.
    *   **VRP Pattern (iframe `allow-downloads`):** Bypassing the lack of `allow-downloads` sandbox flag (VRP: `40060695`, VRP2.txt#11682 - potentially related to `.xht` handler). See [iframe_sandbox.md](iframe_sandbox.md). (See README Tip #5 - Policy Bypass)
    *   **VRP Pattern (iframe `noopener`):** Downloads initiated via `window.open` with `noopener` from sandboxed frames bypassing checks (VRP2.txt#1105). See [iframe_sandbox.md](iframe_sandbox.md).
    *   **VRP Pattern (Drag-and-Drop `file:` URL):** Dropping a `file:` URL that passes initial MIME filter (`FilterURLsForDropability` checks `blink::IsSupportedMimeType`) might lead to RCE if the handler for that allowed file type (e.g., `.xht` VRP#11682) has a vulnerability exploitable from a local file context. (See README Tip #9 - Data Transfer Boundaries)
*   **Extension API (`chrome.downloads`) Abuse:** Extensions using the Downloads API to perform unintended actions due to insufficient validation of extension-provided data. (See README Tip #2 - Extension & DevTools Security)
    *   **VRP Pattern (Local File Read / Path Traversal via `onDeterminingFilename`):** Extensions with only `downloads` permission using `onDeterminingFilename` to suggest a filename containing path traversal components (`..`) or absolute paths. **The primary defense is `net::IsSafePortableRelativePath` (implemented in `filename_util.cc`) called within `ExtensionDownloadsEventRouter::DetermineFilename`. This function checks for `..`, absolute paths, separators within components, reserved names, etc.** Vulnerabilities likely relied on bypassing this check or exploiting weaknesses in downstream components like `BaseFile::MoveFileAndAdjustPermissions` and the OS file move APIs (e.g., Windows `IFileOperation`). (VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975). Interaction with FSA API (VRP: `1428743`). (See README Tip #2 - File Read, Tip #6)
    *   **VRP Pattern (Environment Variable Leak):** Using `chrome.downloads.download` with `saveAs: true` or `chrome.downloads.onDeterminingFilename` to suggest a name containing environment variables (`%VAR%`) to leak their values. **The `filename` option in `downloads.download` undergoes explicit `%` to `_` replacement in `DownloadsDownloadFunction::Run` (fixing VRP 1310461). However, filenames suggested via `onDeterminingFilename` bypass this explicit check and also bypass the general `net::IsSafePortableRelativePath` validation which does not check for `%`.** The leak likely occurs if the "Save As" dialog (if shown) fails to sanitize the name (less likely due to `RemoveEnvVarFromFileName` call in `DownloadTargetDeterminer`) or, more likely, if no dialog is shown and the file is saved with `%VAR%` literally in the name, allowing later expansion by the OS or other applications. (VRP2.txt#2935, #3189, #5603 - referencing 1310461 bypass). (See README Tip #2 - Info Leak, Tip #5 - Timing/Side-Channel Leaks)

## 3. Further Analysis and Potential Issues
*   **Target Determination & File Operations:** Audit the entire flow from accepting an extension suggestion (`NotifyExtensionsDone`) through `DownloadTargetDeterminer` state changes to the final file move in `BaseFile::MoveFileAndAdjustPermissions`. How robust is the final OS move against malicious paths suggested earlier? (See README Tip #6)
*   **Environment Variable Handling:** Confirm how/if the OS or other components expand `%VAR%` when encountering filenames saved via the `onDeterminingFilename` pathway (where `%` isn't sanitized by Chrome). (See README Tip #5 - Timing/Side-Channel Leaks)
*   **UI Origin Display Consistency (Edge Cases):** While standard downloads show Final URL (hostname or origin), investigate how redirects involving non-standard schemes (`blob:`, `filesystem:`, `data:`, `about:srcdoc`) affect the `GetURL()` value passed to formatters and what `url_formatter::FormatUrl` actually returns for these schemes. Does it always align with user expectations or VRP reports (e.g., showing "about:srcdoc")? (See README Tip #1 - Dialog/Prompt Spoofing)
*   **Safe Browsing Integration:** Analyze the interaction between `DownloadManagerImpl` and `DownloadProtectionService`. Are checks always performed when expected? Can checks be bypassed by manipulating download parameters or timing? What happens if the check fails (VRP2.txt#10091 - resulted in no warning)?
*   **`chrome.downloads` API Security:** Path traversal via `onDeterminingFilename` is primarily mitigated by `net::IsSafePortableRelativePath`. Environment variable handling via `onDeterminingFilename` remains the primary concern based on VRP data and code analysis. (VRP2.txt#1176, #3189, etc.). (See README Tip #2, Tip #6)
*   **Sandbox Interactions:** Verify that downloads initiated from sandboxed iframes correctly adhere to the `allow-downloads` flag in all cases (including `noopener` popups - VRP2.txt#1105). Verify the process handling `file:` URL navigations initiated by drag-and-drop. (See README Tip #5, Tip #9)
*   **File System Interaction (FSA):** Analyze interactions between downloads and the File System Access API (VRP: `1428743`). (See README Tip #6, Tip #9)

## 4. Code Analysis
*   `DownloadManagerImpl`: Core class managing downloads.
*   `DownloadTargetDeterminer`: Logic for determining the final download path. Contains the state machine (`DoGenerateTargetPath`, `DoDetermineLocalPath`, `DoDetermineIntermediatePath`, etc.). Relies on delegate and `net::GenerateFileName`. Calls `RemoveEnvVarFromFileName` before showing Save As dialog on Windows.
*   `RenderFrameHostImpl::VerifyDownloadUrlParams`: Static function called during download initiation (e.g., in `RenderFrameHostImpl::DownloadURL`). Validates the download URL (blocking `javascript:` schemes) and checks for path traversal characters (`/`) in filenames suggested via context menu saves (`is_context_menu_save`).
*   `DownloadItemImpl` (`components/download/internal/common/download_item_impl.cc`): Represents download state. Holds URLs (`url_chain_`, `original_url_`, `referrer_url_`) and initiator info.
*   `DownloadItemModel` / `DownloadUIModel`: Wrapper providing UI-focused data. `GetURL()` provides final URL. `GetOriginalURL()` provides original URL. `GetStatusTextForLabel` uses `GetOriginalURL` *only if* `ShouldPromoteOrigin` returns true (which it doesn't by default on desktop). `GetTabProgressStatusText` uses download state/progress, not URLs.
*   `BaseFile` (`components/download/internal/common/base_file.cc`, `base_file_win.cc`, etc.): Handles file IO. `Rename` calls `MoveFileAndAdjustPermissions`.
*   `BaseFile::MoveFileAndAdjustPermissions` (`base_file_win.cc`): Performs the final file move using OS APIs (`IFileOperation` on Win). **Likely does not expand environment variables itself.** Relies on OS API behavior for filename validation (including rejecting `..`).
*   `download::DetermineLocalPath` (`components/download/internal/common/download_utils.cc`): **Performs NO path validation/sanitization for non-Android cases.**
*   `net/base/filename_util.cc`: Contains `net::GenerateFileName`, `net::GenerateSafeFileName`, `net::IsReservedNameOnWindows`, **`net::IsSafePortableRelativePath`, `net::IsSafePortablePathComponent` (which do NOT check for `%`)**.
*   `net/base/filename_util_internal.h`: Declares internal helpers like `IsShellIntegratedExtension`.
*   `net/base/mime_util.cc`: Contains `GetMimeTypeFromFile` based on last extension. `kPrimaryMappings` define `application/xhtml+xml` -> `xhtml,xht,xhtm`.
*   `blink/common/mime_util/mime_util.cc`: Contains `blink::IsSupportedMimeType`, which includes `application/xhtml+xml` as supported.
*   `DownloadShelfView` / `DownloadBubbleRowView`: UI views. Use `DownloadItemModel`. Primarily display filename and status text (which doesn't contain origin on desktop).
*   `DownloadShelfContextMenuView` / `DownloadShelfContextMenu`: UI context menu. Adds standard command items, no direct URL/origin display.
*   `DownloadNotificationManager` / `DownloadItemNotification`: UI notifications. Display title, message (based on status), and progress status (includes **hostname of final URL**).
*   `DownloadsDOMHandler` / `DownloadsListTracker` (`chrome/browser/ui/webui/downloads/`): Backend for `chrome://downloads`. Uses `DownloadListTracker::CreateDownloadData` which calls `FillUrlFields`.
*   `FillUrlFields` / `GetFormattedDisplayUrl` (`chrome/browser/ui/webui/downloads/downloads_list_tracker.cc`): Helper functions. Use `DownloadItem::GetURL()` (final URL) and format it using `url_formatter::FormatUrlForSecurityDisplay`.
*   `url_formatter::FormatUrlForSecurityDisplay` / `FormatOriginForSecurityDisplay` (`components/url_formatter/elide_url.cc`): Format URL/Origin for display. Primarily show origin part, omit path/query/ref for standard URLs. Fallback to `FormatUrl` for non-standard.
*   `url_formatter::FormatUrl` (`components/url_formatter/url_formatter.cc`): General URL formatting. Returns "about:srcdoc" for `about:srcdoc` URLs.
*   `DownloadsApi::DetermineFilenameFunction` / `ExtensionDownloadsEventRouter::DetermineFilename` (`chrome/browser/extensions/api/downloads/downloads_api.cc`): Handles `onDeterminingFilename` listener. Calls `net::IsSafePortableRelativePath` for path traversal sanitization. **Does NOT explicitly sanitize environment variables (`%`).**
*   `DownloadsApi::DownloadFunction` (`chrome/browser/extensions/api/downloads/downloads_api.cc`): Handles `chrome.downloads.download`. **Explicitly replaces `%` with `_` in the `filename` option.**
*   `DownloadProtectionService`: Interface with Safe Browsing.
*   `content/browser/download/download_utils.cc`: Utility functions.
*   `ui/shell_dialogs/select_file_dialog_win.cc`, `execute_select_file_win.cc`: Implements the "Save As" dialog logic on Windows using COM `IFileSaveDialog`/`IFileOpenDialog`. Relies on native dialog behavior for extension handling.
*   `chrome/browser/platform_util_win.cc`, `ui/base/win/shell.cc`: Contains logic (`OpenFileViaShell`) that calls `ShellExecuteExW` to open files/folders using default OS handlers. Used for opening completed downloads.
*   `components/safe_browsing/content/resources/download_file_types.asciipb`: Defines dangerous file types, including `.settingcontent-ms`.

## 5. Areas Requiring Further Investigation
*   **Environment Variable Handling:** Confirm how/if the OS or other components expand `%VAR%` when encountering filenames saved via the `onDeterminingFilename` pathway (where `%` isn't sanitized by Chrome). (See README Tip #5 - Timing/Side-Channel Leaks)
*   **Non-Standard Scheme Formatting:** How exactly does `url_formatter::FormatUrl` handle various non-standard schemes (`blob:`, `filesystem:`, `data:`, `about:srcdoc`) in different contexts (download list, notifications)? Does it always align with user expectations? (VRPs #916831, #11062). (See README Tip #1 - Dialog/Prompt Spoofing)
*   **Complex Redirects:** Can redirects involving Service Workers, data URIs, or specific headers cause `DownloadItem::GetURL()` to return a misleading final URL that bypasses formatting checks?
*   **Safe Browsing Failure Modes:** What happens if the `DownloadProtectionService` check fails or times out? Ensure safe default behavior (VRP2.txt#10091).
*   **Download Bubble/Notification Interactions:** Test the new download bubble/notification UI for clickjacking/keyjacking vulnerabilities (VRP2.txt#9287, #10006, #10086). (See README Tip #1 - Input/Interaction Hijacking)
*   **Tooltips:** Check tooltip generation logic in `DownloadItemModel`, `DownloadShelfView`, `DownloadBubbleRowView`.
*   **XHTML/File Handler Vulnerabilities:** Investigate potential vulnerabilities in Blink's handling of XHTML or other file types allowed through drag-and-drop filtering (VRP#11682). (See README Tip #10 - Parsing & Content Handling)
*   **Save As Dialog Behavior:** Deep dive into `IFileSaveDialog` behavior regarding user input vs. selected filters for extension handling (related to VRPs `1228653`, `1378895`). (See README Tip #6)

## 6. Related VRP Reports
*   **UI Origin Spoofing:** VRP: `1157743`, `1281972`, `1499408`, `916831` (srcdoc); VRP2.txt#2294, #11062, #4900, #8323, #8581 (long subdomain), #12168 (redirect)
*   **UI Interaction:** VRP2.txt#9286 (Keyjacking), #9287 (Clickjacking), #10006, #10086 (Obscuring fullscreen toast)
*   **File Type Masking:** VRP: `1228653` (`.jpg.scf`), `1378895` (`%%` bypass), `1303486` (`.url` files); VRP2.txt#14019 (General), #15866 (`%%` bypass), #12993 (`.url` files)
*   **Dangerous Download Bypass:** VRP2.txt#16391 (DevTools `Page.downloadBehavior`)
*   **Safe Browsing Bypass:** VRP: `1416794`; VRP2.txt#10091 (Large Request Payload)
*   **Silent Download/Write:** VRP: `1243802` (Save Picker + Enter); VRP2.txt#9302 (Save Picker + Enter), #5730 (Web Share UNC)
*   **Sandbox Bypass:** VRP: `40060695` (`allow-downloads`, `.SettingContent-ms` execution); VRP2.txt#11682 (`allow-downloads`, `.xht` RCE), #1105 (`noopener`, drag-and-drop `file:`)
*   **Extension API (`chrome.downloads`):**
    *   *Local File Read / Path Traversal:* VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975 (Via `onDeterminingFilename`. Mitigated primarily by `net::IsSafePortableRelativePath`. Vulnerabilities likely involved bypassing this or exploiting downstream OS API behavior.)
    *   *FSA Interaction:* VRP: `1428743`
    *   *Env Var Leak:* VRP2.txt#2935 (`download` option - fixed), #3189 (`onDeterminingFilename` bypass - path lacks `%` sanitization, relies on OS interaction). See VRP `1310461`, `1247389`.
    *   *SOP Bypass:* VRP2.txt#11328
*   **History Spoofing:** VRP: `1513412`; VRP2.txt#3449

## 7. Cross-References
*   [safe_browsing_service.md](safe_browsing_service.md)
*   [extension_security.md](extension_security.md)
*   [file_system_access.md](file_system_access.md)
*   [iframe_sandbox.md](iframe_sandbox.md)
*   [drag_and_drop.md](drag_and_drop.md)
*   [webshare.md](webshare.md)
*   [url_formatting.md](url_formatting.md)
