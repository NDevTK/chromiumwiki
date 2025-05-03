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
*   **Sandbox Escape via IPC (VRP2.txt#4):** Insufficient validation of drag metadata sent via IPC from a compromised renderer during `RenderWidgetHostImpl::StartDragging` or related handlers. While `StartDragging` filters the actual `DropData` based on permissions, it forwards the renderer-provided `drag_operations_mask` and `event_info` (containing screen location) directly to the delegate view, potentially allowing a compromised renderer to influence the drag operation if the platform handler trusts this metadata:
    ```cpp
    // content/browser/renderer_host/render_widget_host_impl.cc
    void RenderWidgetHostImpl::StartDragging(
        /* ... */, DragOperationsMask drag_operations_mask, /* ... */,
        blink::mojom::DragEventSourceInfoPtr event_info) {
      // ... DropData filtering happens here ...

      RenderViewHostDelegateView* view = delegate_->GetDelegateView();
      // ... checks ...

      // Filtered data, but original mask and event_info are passed down
      view->StartDragging(filtered_data, source_origin, drag_operations_mask, image,
                          offset, rect, *event_info, this);
    }
    ```
*   **File Access via Tampered DropData:** While file permissions are granted late (`PrepareDropDataForChildProcess`), manipulation of file paths *before* this point in `content::DropData` or `ui::OSExchangeData` could potentially influence permission granting or target location.
*   **Renderer Taint Spoofing:** Seems difficult as taint is set browser-side in `MarkRendererTaintedFromOrigin` based on the source context.
*   **SOP Bypass / Information Leak:**
    *   **File Contents:** `IsImageAccessibleFromFrame` relies on the renderer's `ImageResourceContent::IsAccessAllowed()` check performed at drag start. If the resource's accessibility changes mid-drag, the browser check might be stale.
    *   **Download URL / SameSite Bypass (VRP: `40060358` / VRP2.txt#283):** The drag-and-drop mechanism itself correctly passes the initiator origin when setting up the `DownloadURL` request via `DragDownloadFileUI::InitiateDownload`. The bypass likely occurs later in the download or network stack's handling of the request and associated cookies.
    *   **Portal Activation (VRP2.txt#8707):** SOP bypass during portal activation while dragging. The browser-side `IsValidDragTarget` check only verifies `SiteInstanceGroup` match and doesn't account for portal state changes. **The bypass might occur if portal activation changes process relationships mid-drag, invalidating the initial `SiteInstanceGroup` check, or if it compromises renderer-side checks in `blink::DragController` or event handlers.** See [portals.md](portals.md).
*   **Incorrect Data Handling/Filtering:** Flaws in enterprise policy filtering (`HandleOnPerformingDrop`) or Exo data exchange logic (`ChromeDataExchangeDelegate`).
*   **Exo Data Exchange:** Security of the Wayland protocol exchange, potential for pickle parsing vulnerabilities in `ParseFileSystemSources`, lack of taint tracking in `exo::DragDropOperation`.
*   **UI Spoofing:** Misleading drag visuals or drop target indicators.

## 4. Code Analysis
*   `DragController::DoSystemDrag` (Renderer): Packages `DataObject` into `WebDragData`, sends IPC.
*   `DataObject::ToWebDragData` (Renderer): Sets `image_accessible` flag based on `IsImageAccessible()`. Sets `download_metadata`.
*   `ImageResourceContent::IsAccessAllowed()` (Renderer): Called by `DataTransfer::DeclareAndWriteDragImage`. Performs origin/CORS checks. **Result stored at drag start.**
*   `RenderWidgetHostImpl::StartDragging` (Browser): Handles drag start IPC. Filters outgoing data. Calls `WebContentsViewAura::StartDragging`. **Initial validation point for renderer data.**
*   `WebContentsViewAura::StartDragging` (Browser): Prepares `ui::OSExchangeData` via `PrepareDragData`.
*   `PrepareDragData` (Browser): Calls `MarkRendererTaintedFromOrigin`. Handles `download_metadata`. Performs `IsImageAccessibleFromFrame` check (using stored renderer result).
*   `WebContentsViewDragSecurityInfo::IsImageAccessibleFromFrame` (Browser): Returns stored flag based on renderer's check at drag start. **Potentially stale if resource accessibility changes.**
*   `WebContentsViewDragSecurityInfo::IsValidDragTarget` (Browser): **Browser-side check for intra-page drags.** Called on Enter and Drop. Blocks drags between different `SiteInstanceGroup`s. **Does not explicitly check Portal activation state. Relies on `SiteInstanceGroup` remaining valid and renderer checks for finer-grained security within the group.** (Related VRP2.txt#8707).
*   `RenderWidgetHostImpl::GrantFileAccessFromDropData` / `PrepareDropDataForChildProcess` (Browser): Grants isolated file permissions just before final drop IPC. **Key security boundary for file drops.**
*   `ash::DragDropController::StartDragAndDrop` (Ash): Platform drag loop.
*   `exo::DragDropOperation::StartDragDropOperation` (Exo): **Does not set taint flag** when populating `OSExchangeData` from `DataSource`.
*   `ash::ChromeDataExchangeDelegate`: Implements Exo data handling. Calls `ParseFileSystemSources`.
*   `file_manager::util::ParseFileSystemSources` (Ash): Parses pickled `fs/sources` data. **Requires source to be Files App UI.** Potential pickle vulnerability target.
*   `DragDownloadFile` / `DragDownloadFileUI::InitiateDownload` (Browser): Handles `DownloadURL` logic, correctly passing initiator origin to `DownloadManager`. **Vulnerability likely downstream.**
*   `ImageResource::IsAccessAllowed()` (Renderer): Checks frame origin vs resource response (CORS).

## 5. Areas Requiring Further Investigation
*   **Portal Activation vs. Drag State:** Analyze how Portal activation (`Portal::Activate`) interacts with the ongoing drag state managed by `ash::DragDropController` and `blink::DragController`. Does activation change `SiteInstanceGroup` relationships in a way that bypasses the check in `IsValidDragTarget`? Are renderer-side checks in `blink::DragController` robust against activation? (VRP2.txt#8707).
*   **`IsValidDragTarget` Robustness:** Besides Portal activation, are there other ways (e.g., complex iframe navigation, other process switches) the `SiteInstanceGroup` check could become invalid mid-drag before the final drop check?
*   **Exo Taint Tracking:** Investigate the lack of taint tracking when drags originate from Exo (`exo::DragDropOperation`). Could this allow tainted data (e.g., files from web content dropped into an Exo app) to bypass checks?
*   **Exo `ParseFileSystemSources`:** Audit the pickle parsing logic in `file_manager::util::ParseFileSystemSources` for vulnerabilities, although it requires the source to be the Files App.
*   **`IsImageAccessibleFromFrame` Staleness:** Can the accessibility state checked by `ImageResourceContent::IsAccessAllowed()` change between drag start and drop, making the browser check stale?
*   **IPC Validation (`StartDragging`):** Audit validation of metadata in `RenderWidgetHostImpl::StartDragging`. (VRP2.txt#4).
*   **`DownloadURL` / SameSite:** Focus investigation on the network service and cookie handling logic after the download request is initiated by `DragDownloadFileUI::InitiateDownload`. (VRP: `40060358`).

## 6. Related VRP Reports
*   VRP2.txt#4 (Sandbox Escape via Drag & Drop IPC)
*   VRP: `40060358` / VRP2.txt#283 (SameSite via `DownloadURL`)
*   VRP2.txt#8707 (**SOP bypass via Portal activation during drag - likely involves `IsValidDragTarget` check bypass due to process changes or compromised renderer checks**).

## 7. Cross-References
*   [downloads.md](downloads.md)
*   [portals.md](portals.md)
*   [ipc.md](ipc.md)
*   [site_isolation.md](site_isolation.md)
*   [site_instance_group.md](site_instance_group.md)