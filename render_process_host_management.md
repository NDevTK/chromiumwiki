# RenderProcessHost Management

This page details how `RenderProcessHostImpl` objects are managed, including process reuse policies, the role of the `SpareRenderProcessHostManager`, and the handling of process limits.

## Process Reuse Policies

Chromium can reuse renderer processes for different `SiteInstance`s under certain conditions to optimize resource usage. The process reuse policy is determined by the `SiteInstance`'s `ProcessReusePolicy` and other factors such as the process limit and whether the process is suitable for the given `SiteInstance`.

The following process reuse policies are available:

-   `REUSE_PENDING_OR_COMMITTED_SITE_SUBFRAME`: Reuse a process if it has a pending or committed navigation to the same site, and it is suitable for hosting a subframe.
-   `REUSE_PENDING_OR_COMMITTED_SITE_WORKER`: Reuse a process if it has a pending or committed navigation to the same site, and it is suitable for hosting a worker.
-   `REUSE_PENDING_OR_COMMITTED_SITE_WITH_MAIN_FRAME_THRESHOLD`: Reuse a process if it has a pending or committed navigation to the same site, it is suitable for hosting a main frame, and it meets certain thresholds (e.g., memory usage, number of main frames).
-   `PROCESS_PER_SITE`: Always use a dedicated process for each site.
-   `DEFAULT`: Use the default process reuse policy, which may vary depending on the platform and other factors.

## Spare RenderProcessHost

The `SpareRenderProcessHostManager` manages a spare `RenderProcessHost` that can be used for navigations that require a new process. This helps to reduce the latency of process creation when a new process is needed.

-   `SpareRenderProcessHostManagerImpl::Get`: Returns the singleton instance of the `SpareRenderProcessHostManager`.
-   `SpareRenderProcessHostManagerImpl::PrepareForFutureRequests`: Prepares for future navigation requests by creating a spare process if needed.
-   `SpareRenderProcessHostManagerImpl::MaybeTakeSpare`: Attempts to take the spare process for a given navigation.

## Process Limits

Chromium limits the number of renderer processes that can be created. The maximum number of processes is determined by the platform and the amount of available memory.

-   `RenderProcessHost::GetMaxRendererProcessCount`: Returns the maximum number of renderer processes allowed.
-   `RenderProcessHostImpl::IsProcessLimitReached`: Returns true if the process limit has been reached.

## Process Reuse and SiteInstance

The `GetProcessHostForSiteInstance` method is the main entry point for selecting a `RenderProcessHost` for a given `SiteInstance`. It considers the following factors:

1. **Process Reuse Policy**: The policy specified by the `SiteInstance`.
2. **Unmatched Service Workers**: Whether there is a process with an unmatched service worker for the site.
3. **SiteInstanceGroup**: Whether there is a process tracked by the `SiteInstanceGroup` that could be reused.
4. **Existing Process Host**: Whether there is an existing process host that can be reused based on the process reuse policy.
5. **Spare Process**: Whether the spare process can be used.
6. **Process Limit**: Whether the process limit has been reached.

If no suitable process is found, a new `RenderProcessHost` is created.

## Further Investigation

-   The detailed logic of the process reuse policies and how they are applied.
-   The interaction between `RenderProcessHostImpl`, `SiteInstanceImpl`, and `SiteInstanceGroup` in managing process allocation.
-   The mechanisms for determining process suitability and the factors considered.
-   The impact of process reuse on security, performance, and resource usage.
-   The role of the `SpareRenderProcessHostManager` in optimizing process creation.
-   The handling of process limits and the strategies for reusing processes when the limit is reached.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   `content/browser/renderer_host/spare_render_process_host_manager_impl.h`
-   `content/browser/renderer_host/spare_render_process_host_manager_impl.cc`
-   `content/browser/site_instance_impl.h`
-   `content/browser/site_instance_impl.cc`
-   `content/browser/site_instance_group.h`
-   `content/browser/site_instance_group.cc`
