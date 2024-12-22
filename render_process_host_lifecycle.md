# RenderProcessHost Lifecycle

This page details the lifecycle of a `RenderProcessHostImpl` object, including its creation, initialization, use, and destruction.

## Creation

A `RenderProcessHostImpl` object is created when a new renderer process is needed. This can happen due to various reasons, such as:

-   A new navigation to a site that requires a dedicated process.
-   An existing process reaching its process limit.
-   The need for a spare process (if enabled).

The creation is initiated by calling `RenderProcessHostImpl::CreateRenderProcessHost`, which is typically invoked by the `RenderProcessHostFactory` or the `SpareRenderProcessHostManager`.

## Initialization

After creation, the `RenderProcessHostImpl` object is initialized by calling its `Init` method. This involves:

1. Establishing an IPC channel with the renderer process.
2. Creating message filters.
3. Registering Mojo interfaces.
4. Creating a metrics allocator.
5. Sending an initial `RenderProcessReady` notification to observers.

## Use

Once initialized, the `RenderProcessHostImpl` can be used to host one or more `RenderFrameHost`s, which represent individual frames within web pages. The `RenderProcessHostImpl` is responsible for managing the communication between these frames and the browser process.

## States

A `RenderProcessHostImpl` can be in one of the following states:

-   **Unused**: The process has been created but has not yet been used to host any content.
-   **Initialized**: The process has been initialized and is ready to host content.
-   **Active**: The process is currently hosting one or more active frames.
-   **Pending Reuse**: The process is no longer hosting any active frames, but it is being kept alive for potential reuse.
-   **Delayed Shutdown**: The process is no longer hosting any active frames, but its shutdown is being delayed to allow for potential reuse or to give unload handlers a chance to run.
-   **Dead**: The process has exited or crashed.

## Destruction

A `RenderProcessHostImpl` object is destroyed when it is no longer needed. This can happen due to:

-   The renderer process exiting or crashing.
-   All associated `RenderFrameHost`s being destroyed.
-   The `RenderProcessHostImpl` being explicitly shut down by the browser.

The destruction process involves:

1. Notifying observers that the process has exited or been destroyed.
2. Cleaning up resources associated with the process.
3. Deleting the `RenderProcessHostImpl` object.

## Further Investigation

-   The detailed logic of each state transition.
-   The conditions that trigger the creation and destruction of a `RenderProcessHostImpl`.
-   The interaction between `RenderProcessHostImpl` and other components, such as `SiteInstanceGroup` and `NavigationRequest`, during its lifecycle.
-   The handling of edge cases and error conditions during the lifecycle.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
