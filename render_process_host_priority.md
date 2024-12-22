# RenderProcessHost Priority

This page details how process priority is determined and managed in `RenderProcessHostImpl`.

## Priority Factors

The priority of a `RenderProcessHost` is determined by several factors:

-   **Visibility**: Whether the process is hosting visible or hidden content. Visible processes have higher priority.
-   **Frame Depth**: The lowest depth of visible frames, or if there are no visible widgets the lowest depth of hidden frames. Lower depth (closer to the top frame) results in higher priority.
-   **Intersects Viewport**: Whether the process hosts a frame that intersects the viewport.
-   **Media Stream**: Whether the process is capturing or playing a media stream.
-   **Foreground Service Worker**: Whether the process has a foreground service worker.
-   **Boost for Loading**: Whether the process is currently loading a page.
-   **Pending Views**: Whether the process has pending views.
-   **`PriorityOverride`**: An optional priority override set by the embedder.

## Priority Management

The `RenderProcessHostImpl` class manages process priority using the following mechanisms:

-   **`RenderProcessHostPriorityClient`**: An interface that allows clients (e.g., `RenderFrameHostImpl`) to contribute to the process priority.
-   **`UpdateProcessPriorityInputs`**: Called when the priority inputs change (e.g., visibility, frame depth). This method recomputes the priority based on the current inputs.
-   **`UpdateProcessPriority`**: Called to update the actual process priority based on the computed priority inputs. This method sends an IPC message to the renderer process to update its priority.
-   **`priority_`**: A member variable that stores the current priority of the process.

## `RenderProcessHostPriorityClient`

The `RenderProcessHostPriorityClient` interface provides the following methods:

-   `GetPriority`: Returns the priority of the client.
-   `AttachInterfaceProviderToHost`: Attaches an interface provider to the host.

## Further Investigation

-   The detailed logic for computing process priority based on the various factors.
-   The interaction between `RenderProcessHostImpl` and `RenderProcessHostPriorityClient` in managing process priority.
-   The impact of process priority on scheduling and resource allocation.
-   The handling of priority overrides and their implications.
-   The role of process priority in managing the lifecycle of renderer processes.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   `content/public/browser/render_process_host_priority_client.h`
