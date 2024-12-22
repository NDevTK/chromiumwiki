# RenderProcessHost Observers

This page details the observer interfaces related to `RenderProcessHostImpl`, including `RenderProcessHostObserver`, `RenderProcessHostInternalObserver`, and `RenderProcessHostPriorityClient`.

## RenderProcessHostObserver

`RenderProcessHostObserver` is a public interface that allows external components to observe the lifecycle events of a `RenderProcessHost`.

### Methods

-   `RenderProcessHostCreated(RenderProcessHost* host)`: Called when a new `RenderProcessHost` is created.
-   `RenderProcessHostReady(RenderProcessHost* host)`: Called when a `RenderProcessHost` is initialized and ready to be used.
-   `RenderProcessHostDestroyed(RenderProcessHost* host)`: Called when a `RenderProcessHost` is destroyed.
-   `RenderProcessExited(RenderProcessHost* host, const ChildProcessTerminationInfo& info)`: Called when a renderer process associated with a `RenderProcessHost` exits.
-   `RenderProcessHostAllocated(RenderProcessHost* host)`: Called when a `RenderProcessHost` is allocated for a navigation.

## RenderProcessHostInternalObserver

`RenderProcessHostInternalObserver` is an internal interface that allows components within the content module to observe events related to a `RenderProcessHost`.

### Methods

-   `RenderProcessHostReady(RenderProcessHostImpl* host)`: Called when a `RenderProcessHostImpl` is initialized and ready to be used.
-   `RenderProcessHostDestroyed(RenderProcessHostImpl* host)`: Called when a `RenderProcessHostImpl` is destroyed.
-   `RenderProcessHostPriorityChanged(RenderProcessHostImpl* host)`: Called when the priority of a `RenderProcessHostImpl` changes.

## RenderProcessHostPriorityClient

`RenderProcessHostPriorityClient` is an interface that allows clients (e.g., `RenderFrameHostImpl`) to contribute to the process priority of a `RenderProcessHost`.

### Methods

-   `GetPriority()`: Returns the priority of the client.
-   `AttachInterfaceProviderToHost(int render_process_host_id)`: Attaches an interface provider to the host.

## Usage in RenderProcessHostImpl

`RenderProcessHostImpl` maintains two lists of observers:

-   `observers_`: A list of `RenderProcessHostObserver` objects.
-   `internal_observers_`: A list of `RenderProcessHostInternalObserver` objects.

`RenderProcessHostImpl` notifies these observers about various events during the lifecycle of the renderer process.

## Further Investigation

-   The detailed implementation of the observer interfaces and how they are used by different components.
-   The specific events that trigger observer notifications.
-   The role of observers in managing the lifecycle of renderer processes and enforcing site isolation.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   `content/public/browser/render_process_host_observer.h`
-   `content/browser/renderer_host/render_process_host_internal_observer.h`
-   `content/public/browser/render_process_host_priority_client.h`
