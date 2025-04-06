# Component: Downloads

## 1. Component Focus
*   **Functionality:** Manages the downloading of files initiated by user actions, web content, or extensions. Includes determining file paths, handling MIME types, performing security checks (Safe Browsing, dangerous file types), displaying download UI (shelf, bubble, notifications), and interacting with the filesystem. Also covers the `chrome.downloads` extension API.
*   **Key Logic:** Download initiation (`DownloadManagerImpl`), target determination (`DownloadTargetDeterminer`), security checks (`DownloadProtectionService`, dangerous file type checks), UI management (`DownloadShelf`, `DownloadBubbleUIController`), extension API handling (`DownloadsApi`).
*   **Core Files:**
    *   `content/public/browser/download_manager.h`, `content/browser/download/download_manager_impl.cc`
    *   `chrome/browser/download/` (various files like `download_item_model.cc`, `download_target_determiner.cc`, `download_prefs.cc`, `download_commands.cc`)
    *   `chrome/browser/ui/views/download/` (UI views like `download_shelf_view.cc`, `download_bubble_view.cc`)
    *   `chrome/browser/extensions/api/downloads/downloads_api.cc`
    *   `components/safe_browsing/content/browser/download/download_protection_service.h` ([safe_browsing_service.md](safe_browsing_service.md))
    *   `ui/android/java/src/org/chromium/ui/UiUtils.java` (Android UI elements)
    *   `content/browser/safe_browsing/download_protection/ppapi_download_request.cc` (PPAPI downloads)

## 2. Potential Logic Flaws & VRP Relevance
*   **Download UI Spoofing/Obscuring:** Misleading users about the origin, filename, or status of a download.
    *   **VRP Pattern (Origin Spoofing):** Download UI (shelf, bubble, history) showing an incorrect origin, often due to redirects, specific schemes (`about:srcdoc`), or timing issues. (VRP: `1157743`, `1281972`, `1499408`, `916831`; VRP2.txt#2294, #11062, #4900, #8323, #8581). History manipulation leading to fake download notifications (VRP: `1513412`, VRP2.txt#3449).
    *   **VRP Pattern (UI Obscuring/Interaction):** Download notifications obscuring fullscreen toast (VRP2.txt#10006, #10086). Keyjacking/Clickjacking the download bubble/shelf to force opening files (VRP2.txt#9286, #9287).
*   **File Type/Extension Handling:** Bypassing checks intended to warn about or block dangerous file types.
    *   **VRP Pattern (Type Masking):** Using double extensions, special characters (`%%`), or specific save dialog options (`showSaveFilePicker` accept types) to mask the true dangerous extension (e.g., saving `.lnk` or `.scf` as `.jpg`). (VRP: `1228653` - `.jpg.scf`, `1378895` - `%%` bypass, VRP2.txt#14019 - general masking, VRP2.txt#15866 - `%%` bypass via `Save As`). Abusing `.url` files (VRP: `1303486`, VRP2.txt#12993).
    *   **VRP Pattern (Dangerous Download Bypass):** Bypassing checks via DevTools protocol (`Page.downloadBehavior` VRP2.txt#16391) or potentially flaws in `DownloadTargetDeterminer` logic.
*   **Safe Browsing Bypass:** Circumventing Safe Browsing checks for malicious downloads.
    *   **VRP Pattern (Large Data URIs):** Using very large data URIs to cause the Safe Browsing ping to fail (e.g., exceed size limits), potentially skipping the check. (VRP: `1416794`, VRP2.txt#10091). Check interaction with `DownloadProtectionService`. See [safe_browsing_service.md](safe_browsing_service.md).
    *   **VRP Pattern (Referrer Chain):** Safe Browsing ping potentially failing if referrer chain (including data URIs) is too long (related to VRP2.txt#10091).
*   **Silent Downloads / Unintended File Writes:** Downloads occurring or overwriting files without adequate user confirmation or awareness.
    *   **VRP Pattern (Save Picker Interaction):** Using `showSaveFilePicker` combined with Enter key presses to achieve silent downloads/overwrites (VRP: `1243802`, VRP2.txt#9302). See [file_system_access.md](file_system_access.md).
    *   **VRP Pattern (Web Share API):** Using Web Share API with UNC paths (VRP2.txt#5730). See [webshare.md](webshare.md).
*   **Sandbox Bypass:** Downloads initiated from sandboxed contexts bypassing restrictions.
    *   **VRP Pattern (iframe `allow-downloads`):** Bypassing the lack of `allow-downloads` sandbox flag (VRP: `40060695`, VRP2.txt#11682).
    *   **VRP Pattern (iframe `noopener`):** Downloads initiated via `window.open` with `noopener` from sandboxed frames bypassing checks (VRP2.txt#1105). See [iframe_sandbox.md](iframe_sandbox.md).
*   **Extension API (`chrome.downloads`) Abuse:** Extensions using the Downloads API to perform unintended actions.
    *   **VRP Pattern (Local File Read):** Extensions with only `downloads` permission reading arbitrary local files by downloading a local sensitive file (e.g., History, Cookies), using `onDeterminingFilename` to rename it to `.html`, and then opening it to execute embedded script/leak content. Often involves tricks to trigger the initial local file download without `file://` access. (VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975). Requires careful review of `DownloadsApi::DetermineFilenameFunction`. Interaction with FSA API (VRP: `1428743`).
    *   **VRP Pattern (Environment Variable Leak):** Using `chrome.downloads.download` with `saveAs: true` and suggested name containing environment variables (`%VAR%`) to leak their values (VRP2.txt#2935). Using `onDeterminingFilename` to bypass fixes (VRP2.txt#3189).
    *   **VRP Pattern (SOP Bypass):** Downloading cross-origin resources via `chrome.downloads.download` potentially bypassing SOP for reads if combined with drag-drop or other techniques (VRP2.txt#11328).

## 3. Further Analysis and Potential Issues
*   **Target Determination Logic (`DownloadTargetDeterminer`):** Audit the logic for generating final file paths. How are collisions handled (`filename_determination_reason`)? How are dangerous extensions identified and handled (`GetSanitizedFilename`)? Is the logic robust against special characters, long names, path manipulation? How does it interact with `suggestedName` from APIs? (VRP2.txt#15866 bypass).
*   **Download UI Consistency:** Ensure the displayed origin, filename, and security warnings are consistent and accurate across all UI surfaces (shelf, bubble, notifications, `chrome://downloads`) and scenarios (redirects, service worker intercepts, extension-initiated). (VRP: `1157743`, etc).
*   **Safe Browsing Integration:** Analyze the interaction between `DownloadManagerImpl` and `DownloadProtectionService`. Are checks always performed when expected? Can checks be bypassed by manipulating download parameters or timing? What happens if the check fails (VRP2.txt#10091 - resulted in no warning)?
*   **`chrome.downloads` API Security:** Review permission checks and interaction logic for all `chrome.downloads` functions, especially `download()`, `search()`, `getFileIcon()`, and the `onDeterminingFilename` event listener. Ensure extensions cannot bypass file access restrictions or other policies. (VRP2.txt#1176, #4610, #15974, #2935, #3189).
*   **Sandbox Interactions:** Verify that downloads initiated from sandboxed iframes correctly adhere to the `allow-downloads` flag in all cases (including `noopener` popups - VRP2.txt#1105).
*   **File System Interaction (FSA):** Analyze interactions between downloads and the File System Access API (VRP: `1428743`).

## 4. Code Analysis
*   `DownloadManagerImpl`: Core class managing downloads, coordinating with delegates, observers, and protection services.
*   `DownloadTargetDeterminer`: Logic for determining the final download path and filename, including handling dangerous extensions and intermediate names. Contains `GetSanitizedFilename`.
*   `DownloadItemImpl`: Represents a single download item and its state.
*   `DownloadShelfView` / `DownloadBubbleViewController`: UI implementations. Check for UI spoofing/obscuring/interaction vulnerabilities.
*   `DownloadsApi::DetermineFilenameFunction`: Handles `onDeterminingFilename` listener, crucial for extension-based file read vulns (VRP2.txt#1176, etc.) and env var leak bypass (VRP2.txt#3189).
*   `DownloadsApi::DownloadFunction`: Handles `chrome.downloads.download`. Check interaction with `saveAs`, `suggestedName` (VRP2.txt#2935), and permission checks.
*   `DownloadProtectionService`: Interface with Safe Browsing. Check request building (`BuildRequest`) and result handling (`CheckClientDownloadRequestBase::FinishRequest`). (VRP2.txt#10091 - bypass via large data URI / referrer chain).
*   `base/files/file_path.cc::ReplaceExtension`: Filename manipulation logic used in dangerous file checks, had edge case bypass (VRP2.txt#11328).
*   `content/browser/download/download_utils.cc`: Utility functions, including potentially security-relevant checks.

## 5. Areas Requiring Further Investigation
*   **Filename Sanitization:** Deep dive into `DownloadTargetDeterminer::GetSanitizedFilename` and related path/extension handling logic across platforms. Test extensively with special characters (`%%`), multiple extensions, long names, and different file systems.
*   **UI Origin Display:** Trace how the origin URL is determined and displayed in all download UI elements, especially after redirects or for downloads initiated via complex means (Service Workers, extensions, blobs).
*   **Safe Browsing Failure Modes:** What happens if the `DownloadProtectionService` check fails (timeout, network error, large payload)? Ensure this doesn't result in silently allowing potentially dangerous downloads (VRP2.txt#10091).
*   **`chrome.downloads.onDeterminingFilename`:** Audit security checks. Can it overwrite arbitrary files? Does it properly restrict renaming for local file downloads initiated without `file://` permission? (VRP2.txt#1176 etc.).
*   **Download Bubble Interactions:** Test the new download bubble UI for clickjacking/keyjacking vulnerabilities (VRP2.txt#9287).

## 6. Related VRP Reports
*   **UI Origin Spoofing:** VRP: `1157743`, `1281972`, `1499408`, `916831`; VRP2.txt#2294, #11062, #4900, #8323, #8581
*   **UI Interaction:** VRP2.txt#9286 (Keyjacking), #9287 (Clickjacking), #10006, #10086 (Obscuring fullscreen toast)
*   **File Type Masking:** VRP: `1228653` (`.jpg.scf`), `1378895` (`%%` bypass), `1303486` (`.url` files); VRP2.txt#14019 (General), #15866 (`%%` bypass), #12993 (`.url` files)
*   **Dangerous Download Bypass:** VRP2.txt#16391 (DevTools `Page.downloadBehavior`)
*   **Safe Browsing Bypass:** VRP: `1416794`; VRP2.txt#10091 (Large data URI)
*   **Silent Download/Write:** VRP: `1243802` (Save Picker + Enter); VRP2.txt#9302 (Save Picker + Enter), #5730 (Web Share UNC)
*   **Sandbox Bypass:** VRP: `40060695` (`allow-downloads`); VRP2.txt#11682 (`allow-downloads`), #1105 (`noopener`)
*   **Extension API (`chrome.downloads`):**
    *   *Local File Read:* VRP: `1377165`, `989078`; VRP2.txt#1176, #4610, #15974, #11328, #15773, #15975
    *   *FSA Interaction:* VRP: `1428743`
    *   *Env Var Leak:* VRP2.txt#2935 (`download`), #3189 (`onDeterminingFilename` bypass)
    *   *SOP Bypass:* VRP2.txt#11328
*   **History Spoofing:** VRP: `1513412`; VRP2.txt#3449

*(See also [safe_browsing_service.md](safe_browsing_service.md), [extensions_api.md](extensions_api.md), [file_system_access.md](file_system_access.md), [iframe_sandbox.md](iframe_sandbox.md))*
