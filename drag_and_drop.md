# Component: Drag and Drop (Blink DataTransfer & IPC)

## 1. Component Focus
*   **Functionality:** Implements HTML Drag and Drop functionality, allowing users to drag data (text, files, URLs) between different elements or applications. Involves handling drag start, drag over, drop events, managing the `DataTransfer` object in Blink, and relaying drag operations via IPC to the browser process for system-level integration.
*   **Key Logic:** Renderer-side event handling (`EventHandler`), DataTransfer object management (`DataTransfer`), initiating drag via IPC (`WebFrameWidgetImpl::StartDragging -> LocalFrameHost::StartDragging`), Browser-side IPC handling and validation (`RenderWidgetHostImpl::StartDragging`), platform-specific view layer preparing data and initiating OS drag (`WebContentsView*::StartDragging` calls `aura::client::DragDropClient::StartDragAndDrop`), platform drag client managing the drag loop (`ash::DragDropController::StartDragAndDrop`).
*   **Core Files:**
    *   `third_party/blink/renderer/core/clipboard/data_transfer.cc`/`.h`
    *   `third_party/blink/renderer/core/page/drag_controller.cc`/`.h`
    *   `third_party/blink/renderer/core/frame/web_frame_widget_impl.cc` (Initiates IPC)
    *   `content/browser/renderer_host/render_widget_host_impl.cc` (Handles `StartDragging` IPC, performs data validation)
    *   Platform View implementations (e.g., `content/browser/web_contents/web_contents_view_aura.cc` - Calls client's `StartDragAndDrop`)
    *   `ui/aura/client/drag_drop_client.h` (Interface for platform drag implementation)
    *   `ash/drag_drop/drag_drop_controller.cc` (Ash implementation of `DragDropClient`, contains nested run loop)

## 2. Detailed Flow: Drag/Drop onto Web Content

This describes the flow when the drop target is within a web content area managed by `WebContentsViewAura`.

1.  **Initiation & OS Drag:** A system drag starts, and the platform's `DragDropClient` (e.g., `ash::DragDropController`) takes control, receiving `ui::OSExchangeData` (containing data, `allowed_operations`, and `IsRendererTainted` flag) and the initial `screen_location`.
2.  **Entering Web Content View (`WebContentsViewAura::OnDragEntered`):**
    *   Receives `ui::DropTargetEvent`.
    *   Calls `PrepareDropData` to convert `ui::OSExchangeData` -> `content::DropData`. This copies data types and crucially reads `IsRendererTainted` into `drop_data->did_originate_from_renderer`. File paths are copied without validation at this stage. Checks `drag_security_info_.IsImageAccessibleFromFrame()` for `file_contents`.
    *   Async lookup for the target `RenderWidgetHostImpl` (RWHI).
    *   `DragEnteredCallback` receives target RWHI & `DropData`. Validates target, filters `DropData` (e.g., removes filenames if `did_originate_from_renderer` is true), calls `WebContentsDelegate::CanDragEnter`.
    *   Sends `DragTargetDragEnter` IPC to Renderer RWHI, including the original `allowed_operations` mask.
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
        *   Validates target RWHI.
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
*   **Sandbox Escape via IPC (VRP2.txt#4):** Potentially insufficient validation of `StartDragging` IPC message metadata (`event_info.location`, `operations`) before passing to the platform implementation (`ash::DragDropController`). Compromised renderer could influence the drag loop managed by the Ash controller, potentially gaining unintended control.
*   **File Access via Tampered DropData:** The permission granting logic (`PrepareDropDataForChildProcess`) trusts the `DropData` received by `RenderWidgetHostImpl::DragTargetDrop`. If `DropData` could be manipulated between Aura/Views and RWHI (e.g., compromised delegate, bug in data passing), a process could inject arbitrary file paths and gain read access via the `IsolatedContext` mechanism when the drop occurs. The crucial checks are `IsRendererTainted` flag integrity and the `IsolatedContext` isolation itself.
*   **SOP Bypass / Information Leak:**
    *   **File Contents:** `PrepareDropData` uses `drag_security_info_.IsImageAccessibleFromFrame()` to guard against cross-origin access via `file_contents`.
    *   **Download URL:** Possible SameSite bypass using `setData('DownloadURL', ...)` (VRP: `40060358`, VRP2.txt#283).
    *   **Portal Activation:** History of SOP bypass via portal activation during drag (VRP2.txt#8707). See [portals.md](portals.md).
*   **Incorrect Data Handling/Filtering:** Errors in enterprise scanning logic (`HandleOnPerformingDrop`) or other delegate modifications could incorrectly allow/block data. (Updated based on HandleOnPerformingDrop analysis)
*   **UI Spoofing:** Misleading drag visuals or drop target indicators.

## 4. Code Analysis
*   `WebFrameWidgetImpl::StartDragging`: Initiates drag IPC from renderer.
*   `RenderWidgetHostImpl::StartDragging`: Handles drag start IPC. Filters *outgoing* data. Passes metadata to `view_->StartDragging`.
*   `WebContentsViewAura::StartDragging`: Prepares `ui::OSExchangeData`, calls platform `StartDragAndDrop`.
*   `WebContentsViewAura::OnDrag* / PrepareDropData`: Handles incoming drag events, translates `OSExchangeData` to `DropData`, calls `PrepareDropData`.
*   `PrepareDropData`: Reads `IsRendererTainted`, copies data, performs `IsImageAccessibleFromFrame` check.
*   `RenderWidgetHostImpl::DragTarget*` methods: IPC calls to/from renderer for enter/over/leave/drop.
*   `HandleOnPerformingDrop`: Enterprise policy filtering of `DropData`. (Added based on analysis)
*   `RenderWidgetHostImpl::GrantFileAccessFromDropData` / `PrepareDropDataForChildProcess`: Grants file permissions for the drop via `IsolatedContext`. **Key security boundary.** (Added based on analysis)
*   `ash::DragDropController::StartDragAndDrop`: Ash platform implementation, manages nested run loop.
*   `DataTransfer::setData()`: Check handling, especially `DownloadURL`.
*   `DragController::PrepareForDrop()`: Renderer-side logic for drop events.

## 5. Areas Requiring Further Investigation
*   **Integrity of `ui::OSExchangeData`:** How is `IsRendererTainted` set? Can it be spoofed before `PrepareDropData` reads it? Trace `OSExchangeData` creation path (Aura/platform UI code).
*   **`DropData` Tampering Window:** Can delegates (`WebContentsDelegate`, `WebDragDestDelegate`) or other UI components maliciously modify `DropData` before `GrantFileAccessFromDropData` is called? (Updated based on delegate analysis)
*   **`DragSecurityInfo::IsImageAccessibleFromFrame()`:** Detailed analysis of this cross-origin check for `file_contents`.
*   **Platform Drag Loop Behavior (`ash::DragDropController`, etc.):** Validate how parameters like `screen_location`, `allowed_operations` are used within the nested loop and OS interactions (Relates to VRP2.txt#4).
*   **Non-Web Content Delegates:** Analyze `TabDragDropDelegate`, `exo::DataDevice`, etc., for different drag/drop scenarios.
*   **`DownloadURL` Handling:** Deep dive into how `setData('DownloadURL', ...)` triggers downloads via `StartDragging` and its interaction with SameSite cookies. (Moved from Section 3)
*   **Cross-Origin Drag Checks:** Review checks preventing data exposure when dragging cross-origin (`DragController::PrepareForDrop`, browser-side checks). Are they robust against all drag types and scenarios (e.g., involving portals VRP2.txt#8707)? (Moved from Section 3)
*   **Capture Delegate Security:** Review the logic within `TabDragDropDelegate` and `DragDropCaptureDelegate`. (Moved from Section 3)

## 6. Related VRP Reports
*   VRP2.txt#4 (Sandbox Escape via Drag & Drop IPC - potentially unvalidated metadata)
*   VRP: `40060358` / VRP2.txt#283 (SameSite strict cookie bypass via `DownloadURL`)
*   VRP2.txt#8707 (SOP bypass via Portal activation during drag)

*(See also [downloads.md](downloads.md), [navigation.md](navigation.md), [portals.md](portals.md), [ipc.md](ipc.md), [site_isolation.md](site_isolation.md), [permissions.md](permissions.md))*