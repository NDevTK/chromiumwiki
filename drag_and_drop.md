# Component: Drag and Drop (Blink DataTransfer & IPC)

## 1. Component Focus
*   **Functionality:** Implements HTML Drag and Drop functionality, allowing users to drag data (text, files, URLs) between different elements or applications. Involves handling drag start, drag over, drop events, managing the `DataTransfer` object in Blink, and relaying drag operations via IPC to the browser process for system-level integration.
*   **Key Logic:**
    *   **Drag Start:** Renderer-side event handling (`EventHandler`, `DragController`), DataTransfer object management (`DataTransfer`, `DataObject`, `DataObjectItem`), initiating drag via IPC (`DragController::DoSystemDrag` -> `ChromeClient::StartDragging` -> `RenderWidgetHostImpl::StartDragging`). Browser-side handles IPC, validates outgoing data filters, creates `ui::OSExchangeData` (setting taint flag based on source origin via `PrepareDragData`, `MarkRendererTaintedFromOrigin`), calls platform client (`aura::client::DragDropClient::StartDragAndDrop`). Platform client (e.g., `ash::DragDropController`, `exo::DragDropOperation`) manages drag loop/OS interaction.
    *   **Drag Over/Drop (Web Content Target):** Platform client notifies Aura delegate (`WebContentsViewAura`). Delegate translates `ui::OSExchangeData` to `content::DropData` (`PrepareDragData`, checking taint and `IsImageAccessibleFromFrame`). Delegate interacts with `RenderWidgetHostImpl`. RWHI sends events (`DragTargetDragEnter/Over`) to renderer (`blink::FrameWidget`) with source `allowed_operations`. Renderer determines actual operation, sends back via IPC (`OnUpdateDragOperation`). `WebContentsViewDelegate` (`HandleOnPerformingDrop`) may filter data via enterprise policy. Browser grants file permissions (`PrepareDropDataForChildProcess` via `IsolatedContext`) before sending final `DragTargetDrop` IPC to renderer.
    *   **Drag Over/Drop (Exo Target):** Platform client notifies Aura delegate (`exo::DataDevice`). `DataDevice` creates `DataOffer`, populates it via `DataExchangeDelegate` (`SetDropData`), notifies Wayland client (`OnEnter`). Client chooses action, sets it on `DataOffer`. `DataDevice::OnDragUpdated` reads client's action, returns corresponding operation to Aura. On drop, `DataDevice` notifies client (`OnDrop`), waits for client to finish/accept offer, returns final operation to Aura. Data transfer handled via Wayland protocol and `DataExchangeDelegate`.
*   **Core Files:**
    *   `third_party/blink/renderer/core/clipboard/data_transfer.cc`/`.h` & `data_object.cc/.h` & `data_object_item.cc/.h` (Renderer Data Handling)
    *   `third_party/blink/renderer/core/page/drag_controller.cc`/`.h` (Renderer Drag Orchestration)
    *   `third_party/blink/renderer/core/loader/resource/image_resource_content.h/.cc` (Contains `IsAccessAllowed` check)
    *   `third_party/blink/renderer/core/frame/web_frame_widget_impl.cc` (Initiates/Handles Drag IPC)
    *   `content/browser/renderer_host/render_widget_host_impl.cc` (Handles IPC, interacts with View/Delegates, Grants Permissions via Helper)
    *   `content/browser/web_contents/web_contents_view_aura.cc/.h` (Platform View & DragDropDelegate for web content)
    *   `content/browser/web_contents/web_contents_view_drag_security_info.cc/.h` (Handles `IsImageAccessibleFromFrame`, `IsValidDragTarget`)
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
*(Flow description updated)*
1.  **Initiation & OS Drag:** Drag starts externally or internally. If internal (`RenderWidgetHostImpl::StartDragging`), browser filters outgoing data, creates `ui::OSExchangeData`, calls `MarkRendererTaintedFromOrigin`, stores source `SiteInstanceGroupId` in `WebContentsViewDragSecurityInfo`, calls platform `StartDragAndDrop`. Platform client (`ash::DragDropController` etc.) manages OS drag loop.
2.  **Entering Web Content View (`WebContentsViewAura::OnDragEntered`):**
    *   Receives event. Calls `PrepareDropData` (checks taint, image accessibility).
    *   Async RWHI lookup for target under cursor.
    *   `DragEnteredCallback` receives target RWHI & `DropData`. **Validates target using `drag_security_info_.IsValidDragTarget` (checks if source/target RWH share the same `SiteInstanceGroup` for internal drags)**. Filters `DropData` (removes filenames if tainted). Calls `WebContentsDelegate::CanDragEnter`.
    *   Sends `DragTargetDragEnter` IPC to Renderer RWHI.
3.  **Dragging Over Web Content (`WebContentsViewAura::OnDragUpdated`):**
    *   Repeatedly called. Populates `DropData`. Sends `DragTargetDragOver` IPC. Renderer determines operation (`current_op`), sends back IPC. Browser updates cursor.
4.  **Performing the Drop:**
    *   `PerformDropOrExitDrag` -> `PerformDropCallback`.
    *   **Validates target RWHI (`drag_security_info_.IsValidDragTarget` check again).**
    *   Calls `MaybeLetDelegateProcessDrop` -> `HandleOnPerformingDrop` (Enterprise Scan).
    *   If allowed -> `CompleteDrop`.
        *   `RenderWidgetHostImpl::GrantFileAccessFromDropData` -> `PrepareDropDataForChildProcess` (**Grants isolated file permissions**).
        *   Sends final `DragTargetDrop` IPC to renderer with permission-granted `DropData`.
    *   Renderer performs drop action. Source notified via `DragSourceEndedAt`.

## 3. Potential Logic Flaws & VRP Relevance
*   **Sandbox Escape via IPC (VRP2.txt#4):** Potential insufficient validation of `StartDragging` IPC metadata.
*   **File Access via Tampered DropData:** Relies on security of `DropData` path *before* permission granting.
*   **Renderer Taint Spoofing:** Seems difficult.
*   **SOP Bypass / Information Leak:**
    *   **File Contents:** `IsImageAccessibleFromFrame` relies on renderer check.
    *   **Download URL / SameSite Bypass (VRP: `40060358` / VRP2.txt#283):** Likely occurs later in download/network stack.
    *   **Portal Activation (VRP2.txt#8707):** SOP bypass during portal activation while dragging. The browser-side `IsValidDragTarget` check only verifies `SiteInstanceGroup` match and doesn't account for portal state changes. **The bypass might occur if portal activation changes process relationships mid-drag or compromises renderer-side checks.** See [portals.md](portals.md).
*   **Incorrect Data Handling/Filtering:** Enterprise scanning, Exo delegate logic.
*   **Exo Data Exchange:** Delegate security, pickle parsing.
*   **UI Spoofing.**

## 4. Code Analysis
*   `DragController::DoSystemDrag`: Renderer; packages `DataObject` into `WebDragData` via `ToWebDragData`, sends IPC.
*   `DataObject::ToWebDragData`: Renderer; sets `image_accessible` flag based on `DataObjectItem::IsImageAccessible()`. Sets `download_metadata` string for `kMimeTypeDownloadURL`.
*   `DataObjectItem::IsImageAccessible`: Renderer; returns stored boolean flag.
*   `DataTransfer::DeclareAndWriteDragImage` / `WriteImageToDataObject`: Renderer; calls `ImageResourceContent::IsAccessAllowed()`, passes result to `DataObject::AddFileSharedBuffer`.
*   `RenderWidgetHostImpl::StartDragging`: Browser; handles drag start IPC. Filters *outgoing* file/URL data based on source process permissions. Calls `WebContentsViewAura::StartDragging`.
*   `WebContentsViewAura::StartDragging`: Browser; Prepares `ui::OSExchangeData` via `PrepareDragData`, calls platform `StartDragAndDrop`.
*   `PrepareDragData`: Browser; Calls `MarkRendererTaintedFromOrigin`. Handles `download_metadata` via `PrepareDragForDownload` (Windows). Reads `IsRendererTainted`, performs `IsImageAccessibleFromFrame` check.
*   `WebContentsViewDragSecurityInfo::IsImageAccessibleFromFrame`: Browser; Returns stored flag based on renderer's check at drag start (if internal drag), true otherwise.
*   `WebContentsViewDragSecurityInfo::IsValidDragTarget`: **Browser-side check for intra-page drags.** Blocks drags between different `SiteInstanceGroup`s. Relies on renderer (`blink::DragController`) for final origin check within the shared process. **Does not explicitly check Portal activation state.**
*   `ImageResourceContent::IsAccessAllowed()`: Renderer-side origin/CORS check for image resources.
*   `RenderWidgetHostImpl::DragTarget*` methods: Browser; IPC calls to/from renderer for enter/over/leave/drop.
*   `HandleOnPerformingDrop`: Browser; Enterprise policy filtering of `DropData`.
*   `RenderWidgetHostImpl::GrantFileAccessFromDropData` / `PrepareDropDataForChildProcess`: Browser; Grants isolated file permissions. **Key security boundary for file drops.**
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
*   **Integrity of `ui::OSExchangeData` (Non-Renderer Sources).**
*   **`DropData` Tampering Window.**
*   **`ImageResourceContent::IsAccessAllowed()` implementation.**
*   **Platform Drag Loop Behavior (`ash::DragDropController`, etc.) interaction with Portals.** (Relates to VRP2.txt#8707).
*   **Renderer-side drag logic (`blink::DragController`) interaction with Portal activation.** (Relates to VRP2.txt#8707).
*   **Exo `DataExchangeDelegate` File Handling.**
*   **`DownloadURL` / SameSite analysis in network stack.**
*   **Capture Delegate Security.**

## 6. Related VRP Reports
*   VRP2.txt#4 (Sandbox Escape via Drag & Drop IPC)
*   VRP: `40060358` / VRP2.txt#283 (SameSite via `DownloadURL`)
*   VRP2.txt#8707 (**SOP bypass via Portal activation during drag - likely involves process changes or renderer-side check bypass, as browser check `IsValidDragTarget` doesn't handle portal state**).

*(See also [downloads.md](downloads.md), [portals.md](portals.md), [ipc.md](ipc.md), [site_isolation.md](site_isolation.md))*