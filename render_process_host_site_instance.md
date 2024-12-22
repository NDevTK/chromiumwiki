# RenderProcessHost and SiteInstance

This page details the relationship between `RenderProcessHostImpl` and `SiteInstanceImpl`, including how they are associated and how they interact during process allocation and navigation.

## Association between RenderProcessHost and SiteInstance

A `RenderProcessHost` can be associated with multiple `SiteInstance` objects, but a `SiteInstance` is always associated with a single `RenderProcessHost`. This association is crucial for enforcing site isolation, as it ensures that frames from the same site are rendered in the same process, while frames from different sites are isolated into separate processes.

The association between a `RenderProcessHost` and a `SiteInstance` is established when a navigation occurs. The `NavigationRequest` determines the appropriate `SiteInstance` for the navigation, and then a suitable `RenderProcessHost` is selected or created based on the `SiteInstance`'s process reuse policy and other factors.

## Process Allocation

When a new `RenderProcessHost` is needed, the `GetProcessHostForSiteInstance` method is called. This method considers various factors to determine whether an existing process can be reused or a new process needs to be created. One of the key factors is the `SiteInstance` associated with the navigation.

The `SiteInstance`'s `GetSiteInfo` method is used to determine the site URL and other relevant information, such as whether the site requires a dedicated process or if it is for a guest. This information, along with the `IsolationContext`, is used to check if an existing `RenderProcessHost` is suitable for the `SiteInstance` using the `MayReuseAndIsSuitable` method.

## Process Locks

`RenderProcessHostImpl` uses process locks to enforce site isolation. A process lock restricts a renderer process to a specific site or origin. The `SetProcessLock` and `GetProcessLock` methods are used to manage the process lock for a `RenderProcessHost`.

When a navigation occurs, the `NavigationRequest` determines the appropriate process lock based on the `SiteInfo` and other factors. The `RenderProcessHostImpl` then sets the process lock accordingly, ensuring that the renderer process is restricted to the correct site or origin.

## Further Investigation

-   The detailed logic of how `RenderProcessHostImpl` and `SiteInstanceImpl` interact during process allocation.
-   The mechanisms for determining process suitability based on `SiteInstance` properties.
-   The impact of process locks on site isolation and security.
-   The handling of special cases, such as navigations to about:blank or data URLs.
-   The interaction between `RenderProcessHostImpl`, `SiteInstanceImpl`, and `NavigationRequest` during the navigation lifecycle.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   `content/browser/site_instance_impl.h`
-   `content/browser/site_instance_impl.cc`
-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
