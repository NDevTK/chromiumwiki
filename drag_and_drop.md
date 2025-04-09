# Component: Drag and Drop (Blink DataTransfer & IPC)

## 1. Component Focus
*   **Functionality:** Implements HTML Drag and Drop functionality, allowing users to drag data (text, files, URLs) between different elements or applications. Involves handling drag start, drag over, drop events, managing the `DataTransfer` object in Blink, and relaying drag operations via IPC to the browser process for system-level integration.
*   **Key Logic:**
    *   **Drag Start:** Renderer-side event handling (`EventHandler`, `DragController`), DataTransfer object management (`DataTransfer`, `DataObject`, `DataObjectItem`), initiating drag via IPC (`DragController::DoSystemDrag` -> `ChromeClient::StartDragging` -> `RenderWidgetHostImpl::StartDragging`). Browser-side handles IPC, validates outgoing data filters, creates `ui::OSExchangeData` (setting taint flag based on source origin via `PrepareDragData`, `MarkRendererTaintedFromOrigin`), calls platform client (`aura::client::DragDropClient::StartDragAndDrop`). Platform client (e.g., `ash::DragDropController`, `exo::DragDropOperation`) manages drag loop/OS interaction.
    *   **Drag Over/Drop (Web Content Target):** Platform client notifies Aura delegate (`WebContentsViewAura`). Delegate translates `ui::OSExchangeData` to `content::DropData` (`PrepareDropData`, checking taint and `IsImageAccessibleFromFrame`). Delegate interacts with `RenderWidgetHostImpl`. RWHI sends events (`DragTargetDragEnter/Over`) to renderer (`blink::FrameWidget`) with source `allowed_operations`. Renderer determines actual operation, sends back via IPC (`OnUpdateDragOperation`). `WebContentsViewDelegate` (`HandleOnPerformingDrop`) may filter data via enterprise policy. Browser grants file permissions (`PrepareDropDataForChildProcess` via `IsolatedContext`) before sending final `DragTargetDrop` IPC to renderer.
    *   **Drag Over/Drop (Exo Target):** Platform client notifies Aura delegate (`exo::DataDevice`). `DataDevice` creates `DataOffer`, populates it via `DataExchangeDelegate` (`SetDropData`), notifies Wayland client (`OnEnter`). Client chooses action, sets it on `DataOffer`. `DataDevice::OnDragUpdated` reads client's action, returns corresponding operation to Aura. On drop, `DataDevice` notifies client (`OnDrop`), waits for client to finish/accept offer, returns final operation to Aura. Data transfer handled via Wayland protocol and `DataExchangeDelegate`.
*   **Core Files:**
    *   `third_party/blink/renderer/core/clipboard/data_transfer.cc`/`.h` & `data_object.cc/.h` & `data_object_item.cc/.h` (Renderer Data Handling)
    *   `third_party/blink/renderer/core/page/drag_controller.cc`/`.h` (Renderer Drag Orchestration)
    *   `third_party/blink/renderer/core/loader/resource/image_resource_content.h/.cc` (Contains `IsAccessAllowed` check)
    *   `third_party/blink/renderer/core/frame/web_frame_widget_impl.cc` (Initiates/Handles Drag IPC)
    *   `content/browser/renderer_host/render_widget_host_impl.cc` (Handles IPC, interacts with View/Delegates, Grants Permissions via Helper)
    *   `content/browser/web_contents/web_contents_view_aura.cc/.h` (Platform View & DragDropDelegate for web content)
    *   `content/browser/web_contents/web_contents_view_drag_security_info.cc/.h` (Handles `IsImageAccessibleFromFrame`)
    *   `chrome/browser/ui/tab_contents/chrome_web_contents_view_handle_drop.cc/.h` (Enterprise policy handling for drops)
    *   `content/browser/file_system/browser_file_system_helper.cc/.h` (Contains `PrepareDropDataForChildProcess` for permission granting)
    *   `chrome/browser/ash/file_manager/path_util.cc/.h` (Contains `ParseFileSystemSources`)
    *   `content/browser/child_process_security_policy_impl.cc` (Policy enforcement for file access)
    *   `ui/base/dragdrop/os_exchange_data.h/.cc` & `os_exchange_data_provider_win.cc/.h` (Platform-neutral data representation & Taint flag on Windows)
    *   `ui/aura/client/drag_drop_client.h` (Interface for platform drag implementation)
    *   `ui/aura/client/drag_drop_delegate.h` (Interface for drop targets)
    *   `ash/drag_drop/drag_drop_controller.cc/.h` (Ash implementation of `DragDropClient`)
    *   `components/exo/data_device.cc/.h` (Exo implementation of `DragDropDelegate`)
    *   `components/exo/seat.cc/.h` (Manages Exo input state, owns DataExchangeDelegate)
    *   `components/exo/data_exchange_delegate.h` (Interface for Exo data handling)
    *   `chrome/browser/ash/exo/chrome_data_exchange_delegate.cc/.h` (Ash implementation of `DataExchangeDelegate`)
    *   `components/exo/drag_drop_operation.cc/.h` (Handles drag initiation from Exo)
    *   `components/exo/server/wayland_server_controller.cc/.h` (Creates exo::Display and injects delegate)
    *   `chrome/browser/exo_parts.cc` (Creates WaylandServerController with ChromeDataExchangeDelegate)
    *   `content/browser/download/drag_download_file.cc/.h` (Handles `DownloadURL` drag initiation on Windows)
    *   `content/browser/download/download_manager_impl.cc/.h` (Receives `DownloadURL` requests)

## 2. Detailed Flow: Drag/Drop onto Web Content

This describes the flow when the drop target is within a web content area managed by `WebContentsViewAura`.

1.  **Initiation & OS Drag:** A system drag starts (e.g., from OS, another app, browser UI). The platform's `DragDropClient` (e.g., `ash::DragDropController`) takes control, receiving `ui::OSExchangeData` (populated by the source; **will not** have the renderer taint format set for non-renderer sources) and the initial `screen_location`. (If initiated *from* a renderer, `WebContentsViewAura::StartDragging` creates the `OSExchangeData` and calls `MarkRendererTaintedFromOrigin` on its provider before calling `StartDragAndDrop`).
2.  **Entering Web Content View (`WebContentsViewAura::OnDragEntered`):**
    *   Receives `ui::DropTargetEvent`.
    *   Calls `PrepareDropData` to convert `ui::OSExchangeData` -> `content::DropData`.
        *   Reads taint status via `data.IsRendererTainted()` (checks for presence of taint format) into `drop_data->did_originate_from_renderer`.
        *   Copies standard data types. Handles `download_metadata` via `PrepareDragForDownload` (Windows-only).
        *   Checks `drag_security_info_.IsImageAccessibleFromFrame()` before copying `file_contents`. This returns `true` if the drag originated externally, or returns the stored `image_accessible_from_frame_` flag if initiated internally (this flag reflects the result of `ImageResourceContent::IsAccessAllowed()` performed in the renderer at drag start).
        *   Copies filenames/filesystem data without validation *at this stage*.
    *   Async lookup for the target `RenderWidgetHostImpl` (RWHI).
    *   `DragEnteredCallback` receives target RWHI & `DropData`. Validates target (`drag_security_info_.IsValidDragTarget`), filters `DropData` (e.g., removes filenames if `did_originate_from_renderer` is true), calls `WebContentsDelegate::CanDragEnter`.
    *   Sends `DragTargetDragEnter` IPC to Renderer RWHI, including the original `allowed_operations` mask from the source `OSExchangeData`.
3.  **Dragging Over Web Content (`WebContentsViewAura::OnDragUpdated`):**
    *   Repeatedly called. Populates `DropData` via `PrepareDropData`.
    *   Async RWHI lookup.
    *   Sends `DragTargetDragOver` IPC to Renderer RWHI with `allowed_operations` mask.
    *   **Renderer Determines Operation:** Renderer decides the effective operation (`current_op`) based on drop target state, script, and the received mask.
    *   Renderer sends `current_op` back via IPC.
    *   `RenderWidgetHostImpl::OnUpdateDragOperation` receives `current_op`.
    *   `WebContentsViewAura` stores `current_op` in `current_drag_data_->operation`.
    *   `WebContentsViewAura::OnDragUpdated` returns `current_op` to the `DragDropClient`, which updates the cursor.
4.  **Performing the Drop:**
    *   User releases input.
    *   `WebContentsViewAura::GetDropCallback` is called, returning a callback that wraps `PerformDropOrExitDrag`.
    *   `PerformDropOrExitDrag` initiates async RWHI lookup.
    *   `PerformDropCallback` receives target RWHI, final metadata, `ui::OSExchangeData`.
        *   Validates target RWHI (`drag_security_info_.IsValidDragTarget`).
        *   Retrieves/validates `current_drag_data_`.
        *   Calls `MaybeLetDelegateProcessDrop`.
            *   Calls `WebContentsDelegate::OnPerformingDrop` (handled by `HandleOnPerformingDrop` in Chrome).
                *   **Enterprise Scan:** Checks DLP/content analysis policies. May block the drop (`std::nullopt`) or filter `drop_data->filenames` asynchronously based on scan results.
                *   Invokes `GotModifiedDropDataFromDelegate` with original/filtered data or `std::nullopt`.
            *   `GotModifiedDropDataFromDelegate`: If drop is allowed, calls `CompleteDrop` with final `DropData`.
        *   `CompleteDrop`:
            *   Calls `RenderWidgetHostImpl::GrantFileAccessFromDropData`.
                *   Calls `PrepareDropDataForChildProcess`.
                    *   **Permission Granting:** For normal files (`filenames`): Calls `PrepareDataTransferFilenamesForChildProcess`.
                        *   Grants request permission (`GrantRequestOfSpecificFile`).
                        *   Grants read permission if not already present (`GrantReadFile`).
                        *   Registers files with `IsolatedContext`.
                        *   Grants read permission to the isolated filesystem (`GrantReadFileSystem`).
                    *   For File System API files (`file_system_files`):
                        *   Asserts they are not sandboxed types.
                        *   Registers each with `IsolatedContext`.
                        *   Grants read permission to the isolated filesystem (`GrantReadFileSystem`).
                        *   Rewrites the URL in `DropData` to the isolated URL.
            *   Sends `DragTargetDrop` IPC to the renderer (`RenderWidgetHostImpl`) with the final, permission-granted `DropData`.
            *   Calls `WebDragDestDelegate::OnDrop`.
    *   Renderer performs the final drop action within the web content.
    *   `DragSourceEndedAt` / `DragSourceSystemDragEnded` messages are sent back to the original drag source RWH (if applicable) to notify it of the final operation.

## 3. Potential Logic Flaws & VRP Relevance
*   **Sandbox Escape via IPC (VRP2.txt#4):** Potentially insufficient validation of `StartDragging` IPC message metadata (`event_info.location`, `operations`) before passing to the platform implementation (`ash::DragDropController`). Compromised renderer could influence the drag loop managed by the Ash controller, potentially gaining unintended control. The analysis of `ash::DragDropController` suggests it relies on the delegate for operation decisions and real events for location, making this vector less likely within the controller itself, but potentially exploitable via delegate logic or OS interaction.
*   **File Access via Tampered DropData:** The permission granting logic (`PrepareDropDataForChildProcess`) trusts the `DropData` received by `RenderWidgetHostImpl::DragTargetDrop`. If `DropData` could be manipulated between Aura/Views and RWHI (e.g., compromised delegate, bug in data passing), a process could inject arbitrary file paths and gain read access via the `IsolatedContext` mechanism when the drop occurs.
*   **Renderer Taint Spoofing:** The `IsRendererTainted` flag check in `PrepareDropData` relies on the presence/absence of a custom format set by `MarkRendererTaintedFromOrigin` when the drag is initiated *by the browser* in response to a renderer IPC. Drags originating outside the renderer (OS, Views UI, Exo) do not set this format and are correctly identified as non-tainted. Spoofing seems difficult unless the origin identification in `RenderWidgetHostImpl::StartDragging` is flawed or OS-level data can be tampered with. Drags initiated from Exo clients (`exo::DragDropOperation`) correctly *do not* set the taint flag.
*   **SOP Bypass / Information Leak:**
    *   **File Contents:** `IsImageAccessibleFromFrame` check relies on the renderer's `ImageResourceContent::IsAccessAllowed()` result from drag start for same-page drags; returns true for external drags. A compromised renderer could potentially lie about this flag, allowing raw image data into the browser process `DropData`, although drop might still be blocked by renderer SOP checks.
    *   **Download URL / SameSite Bypass (VRP: `40060358` / VRP2.txt#283):** Drags using `setData('downloadurl', ...)` initiate a download via `PrepareDragForDownload` (Windows-only) -> `DragDownloadFile` -> `DownloadManager::DownloadUrl`. This path preserves the initiator origin but marks the source as `DRAG_AND_DROP`. The SameSite bypass likely occurs later in the download/network stack where the `DRAG_AND_DROP` source might cause SameSite cookies to be attached incorrectly based on the initiator origin, potentially treating it like a browser-initiated action rather than a renderer-initiated one in a cross-site context.
    *   **Portal Activation:** History of SOP bypass via portal activation during drag (VRP2.txt#8707). See [portals.md](portals.md).
*   **Incorrect Data Handling/Filtering:** Errors in enterprise scanning logic (`HandleOnPerformingDrop`), `ParseFileSystemSources` (though restricted to Files App source), or other delegate modifications could incorrectly allow/block data.
*   **Exo Data Exchange:** Security relies on `DataExchangeDelegate` (e.g., `ChromeDataExchangeDelegate`) correctly identifying endpoints and handling data (`ParseFileSystemSources`) between Wayland clients and Ash/OS. Incorrect endpoint type determination could lead to policy bypasses. Pickle parsing (`ParseFileSystemSources`) needs careful validation, though it's restricted to Files App source in this path.
*   **UI Spoofing:** Misleading drag visuals or drop target indicators.

## 4. Code Analysis
*   `DragController::DoSystemDrag`: Renderer; packages `DataObject` into `WebDragData` via `ToWebDragData`, sends IPC.
*   `DataObject::ToWebDragData`: Renderer; sets `image_accessible` flag based on `DataObjectItem::IsImageAccessible()`. Sets `download_metadata` string for `kMimeTypeDownloadURL`.
*   `DataObjectItem::IsImageAccessible`: Renderer; returns stored boolean flag.
*   `DataTransfer::DeclareAndWriteDragImage` / `WriteImageToDataObject`: Renderer; calls `ImageResourceContent::IsAccessAllowed()`, passes result to `DataObject::AddFileSharedBuffer`.
*   `RenderWidgetHostImpl::StartDragging`: Browser; handles drag start IPC. Filters *outgoing* file/URL data based on source process permissions. Calls `WebContentsViewAura::StartDragging`.
*   `WebContentsViewAura::StartDragging`: Browser; Prepares `ui::OSExchangeData` via `PrepareDragData`, calls platform `StartDragAndDrop`.
*   `PrepareDragData`: Browser; Calls `MarkRendererTaintedFromOrigin`. Handles `download_metadata` via `PrepareDragForDownload` (Windows). Reads `IsRendererTainted`, performs `IsImageAccessibleFromFrame` check.
*   `WebContentsViewDragSecurityInfo::IsImageAccessibleFromFrame`: Browser; Returns stored flag based on renderer's check at drag start (if internal drag), true otherwise.
*   `ImageResourceContent::IsAccessAllowed()`: Renderer-side origin/CORS check for image resources.
*   `RenderWidgetHostImpl::DragTarget*` methods: Browser; IPC calls to/from renderer for enter/over/leave/drop.
*   `HandleOnPerformingDrop`: Browser; Enterprise policy filtering of `DropData`.
*   `RenderWidgetHostImpl::GrantFileAccessFromDropData` / `PrepareDropDataForChildProcess`: Browser; Grants file permissions for the drop via `IsolatedContext`. **Key security boundary.**
*   `ash::DragDropController::StartDragAndDrop`: Ash; platform implementation, manages nested run loop. Relies on delegates for operation decisions.
*   `exo::DragDropOperation::StartDragDropOperation`: Exo; Populates `OSExchangeData` from `DataSource`, **does not set taint flag**, calls Ash `StartDragAndDrop`.
*   `exo::DataDevice`: Exo; Implements `DragDropDelegate`, gets data via `DataExchangeDelegate`, relies on client for action.
*   `ash::ChromeDataExchangeDelegate`: Ash; Implements `DataExchangeDelegate`, determines endpoint type, calls `ParseFileSystemSources`.
*   `file_manager::util::ParseFileSystemSources`: Ash; Parses pickled `fs/sources` data, **requires source to be Files App UI**.
*   `DragDownloadFile`: Browser; Initiates download via `DownloadManager` based on parsed `download_metadata`. Passes initiator origin.
*   `DownloadManagerImpl`: Browser; Receives download request, determines factory/partition, passes to `InProgressDownloadManager`.
*   `DataTransfer::setData('downloadurl', ...)`: Renderer; Creates a `kStringKind` `DataObjectItem` with type `kMimeTypeDownloadURL` and data formatted as `mime:filename:url`.
*   `DataObject::ToWebDragData`: Renderer; Packages the `kMimeTypeDownloadURL` item as a generic `WebDragData::StringItem`.
*   `RenderWidgetHostImpl::StartDragging`: Browser; Receives the IPC, converts `WebDragData` back to `DropData` (populating `drop_data.download_metadata`). Filters other data types based on permissions. Calls `view->StartDragging`.
*   `WebContentsViewAura::StartDragging`: Browser; Calls `PrepareDragData`.
*   `PrepareDragData`: Browser; (Windows-specific) Checks `drop_data.download_metadata`. If present, calls `PrepareDragForDownload`.
*   `PrepareDragForDownload`: Browser (Windows); Parses `download_metadata`, creates temporary file path, creates `DragDownloadFile` object (passing parsed URL, initiator origin, etc.), calls `provider->SetDownloadFileInfo`.
*   `DragDownloadFile::Start`: Browser (UI Thread); Called when drop target requests file. Calls `DragDownloadFileUI::InitiateDownload`.
*   `DragDownloadFileUI::InitiateDownload`: Browser (UI Thread); Creates `DownloadUrlParameters` (including original initiator origin, parsed URL, referrer), sets source to `DRAG_AND_DROP`, calls `DownloadManager::DownloadUrl`.
*   `DownloadManagerImpl::DownloadUrl` / `BeginDownloadInternal` / `BeginResourceDownloadOnChecksComplete`: Browser; Determines `StoragePartition`, selects appropriate `URLLoaderFactory`, calls `InProgressDownloadManager::BeginDownload`, passing original parameters. **SameSite bypass likely occurs after this point in download/network stack.**
*   `ImageResourceContent::IsAccessAllowed()`: Renderer-side; Called by `WriteImageToDataObject`. Delegates to `ImageResource::IsAccessAllowed`.
*   `ImageResource::IsAccessAllowed()`: Renderer-side; Returns `true` only if (1) the image's internal frame is single-origin AND (2) the resource's network response passes the `IsCorsSameOrigin()` check (standard CORS validation).

## 5. Areas Requiring Further Investigation
*   **Integrity of `ui::OSExchangeData` (Non-Renderer Sources):** Trace `OSExchangeData` creation path for drags originating from Views UI (e.g., tab dragging via `DragUtilsAura` -> `widget->RunShellDrag`) or the OS itself (platform-specific `DragDropClient` implementations like `DesktopDragDropClientWin`) to confirm taint flag is correctly *not* set.
*   **`DropData` Tampering Window:** Can delegates (`WebContentsDelegate`, `WebDragDestDelegate`) or other UI components maliciously modify `DropData` before `GrantFileAccessFromDropData` is called?
*   **`ImageResourceContent::IsAccessAllowed()`:** Detailed analysis of this same-origin check implementation in Blink.
*   **Platform Drag Loop Behavior (`ash::DragDropController`, etc.):** Validate how parameters like `screen_location`, `allowed_operations` are used within the nested loop and OS interactions (Relates to VRP2.txt#4).
*   **Exo `DataExchangeDelegate` File Handling:** How does `ChromeDataExchangeDelegate` handle file paths from various sources (e.g., Lacros via `exo::DragDropOperation`) beyond the Files App pickle format? Does `DataOffer::SetDropData` involve permission checks?
*   **`DownloadURL` / SameSite:** Analyze `DownloadManager` / network stack handling of `DRAG_AND_DROP` source downloads w.r.t cookie attachment policy using the provided `initiator_origin`.
*   **Capture Delegate Security:** Review the logic within `TabDragDropDelegate` and `DragDropCaptureDelegate`.

## 6. Related VRP Reports
*   VRP2.txt#4 (Sandbox Escape via Drag & Drop IPC - potentially unvalidated metadata)
*   VRP: `40060358` / VRP2.txt#283 (SameSite strict cookie bypass via `DownloadURL`)
*   VRP2.txt#8707 (SOP bypass via Portal activation during drag)

*(See also [downloads.md](downloads.md), [navigation.md](navigation.md), [portals.md](portals.md), [ipc.md](ipc.md), [site_isolation.md](site_isolation.md), [permissions.md](permissions.md))*