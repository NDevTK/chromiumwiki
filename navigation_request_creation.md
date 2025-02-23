# NavigationRequest Creation

This page details the different ways a `NavigationRequest` can be created and the parameters used in each case.

## Types of Navigation Requests

There are three main types of navigation requests:

1. **Browser-initiated**: These navigations are initiated by the browser process, typically in response to user actions in the UI, such as clicking a link, submitting a form, or using the address bar.
2. **Renderer-initiated**: These navigations are initiated by the renderer process, typically in response to JavaScript code running in a web page.
3. **Synchronous renderer commits**: These are special cases of renderer-initiated navigations that commit synchronously in the renderer process, such as same-document navigations and `about:blank` navigations.

## Creation Methods

The `NavigationRequest` class provides the following static methods for creating navigation requests:

-   `NavigationRequest::CreateBrowserInitiated`: Creates a new `NavigationRequest` for a browser-initiated navigation.
-   `NavigationRequest::CreateRendererInitiated`: Creates a new `NavigationRequest` for a renderer-initiated navigation.
-   `NavigationRequest::CreateForSynchronousRendererCommit`: Creates a new `NavigationRequest` for a synchronous renderer commit.

## Parameters

The creation methods take various parameters that describe the navigation, including:

-   `frame_tree_node`: The `FrameTreeNode` associated with the navigation.
-   `common_params`: Common parameters for the navigation, such as the URL, referrer, transition type, and whether the navigation has a user gesture.
-   `begin_params`: Parameters for beginning the navigation, such as request headers and load flags.
-   `commit_params`: Parameters for committing the navigation, such as the response headers, the origin to commit, and the CSP.
-   `browser_initiated`: A boolean indicating whether the navigation is browser-initiated.
-   `was_opener_suppressed`: A boolean indicating whether the opener was suppressed for this navigation.
-   `initiator_frame_token`: The frame token of the initiator document, if available.
-   `initiator_process_id`: The process ID of the initiator document, if available.
-   `extra_headers`: Extra headers to be added to the navigation request.
-   `frame_entry`: The `FrameNavigationEntry` associated with the navigation.
-   `entry`: The `NavigationEntryImpl` associated with the navigation.
-   `is_form_submission`: A boolean indicating whether the navigation is a form submission.
-   `navigation_ui_data`: The `NavigationUIData` for the navigation.
-   `impression`: An optional `blink::Impression` associated with the navigation.
-   `initiator_activation_and_ad_status`: The activation and ad status of the initiator.
-   `is_pdf`: A boolean indicating whether the navigation is for a PDF.
-   `is_embedder_initiated_fenced_frame_navigation`: A boolean indicating whether the navigation is an embedder-initiated fenced frame navigation.
-   `is_container_initiated`: A boolean indicating whether the navigation is container-initiated.
-   `has_rel_opener`: A boolean indicating whether the navigation has the `rel="opener"` attribute.
-   `storage_access_api_status`: The status of the Storage Access API for the navigation.
-   `embedder_shared_storage_context`: The shared storage context for the navigation.
-   `current_history_list_offset`: The current history list offset.
-   `current_history_list_length`: The current history list length.
-   `override_user_agent`: A boolean indicating whether the user agent should be overridden.
-   `blob_url_loader_factory`: A `SharedURLLoaderFactory` for loading blob URLs.
-   `navigation_client`: A `NavigationClient` associated with the navigation.
-   `prefetched_signed_exchange_cache`: A cache for prefetched signed exchanges.
-   `renderer_cancellation_listener`: A listener for renderer-initiated navigation cancellations.
-   `render_frame_host`: The `RenderFrameHostImpl` associated with the navigation.
-   `is_same_document`: A boolean indicating whether the navigation is same-document.
-   `origin`: The origin of the navigation.
-   `initiator_base_url`: The base URL of the initiator.
-   `isolation_info_for_subresources`: The `IsolationInfo` for subresources.
-   `referrer`: The referrer for the navigation.
-   `transition`: The page transition type.
-   `should_replace_current_entry`: A boolean indicating whether the navigation should replace the current entry.
-   `method`: The HTTP method for the navigation.
-   `has_transient_activation`: A boolean indicating whether the navigation has transient user activation.
-   `is_overriding_user_agent`: A boolean indicating whether the user agent is being overridden.
-   `redirects`: A vector of redirect URLs.
-   `original_url`: The original URL of the navigation.
-   `coep_reporter`: A reporter for Cross-Origin Embedder Policy violations.
-   `http_response_code`: The HTTP response code.

## Security Considerations in Creation

The creation of a `NavigationRequest` is the first critical step in the navigation lifecycle from a security perspective. Several aspects during creation are crucial for ensuring secure navigations:

-   **Browser-initiated vs. Renderer-initiated**: Browser-initiated navigations are generally more trusted as they originate from direct user actions. Renderer-initiated navigations, however, require more stringent security checks to prevent malicious or unexpected navigations triggered by compromised or malicious web content. Differentiating these types at creation is essential for applying appropriate security policies later in the lifecycle.
-   **Parameter Validation**: Validating parameters during `NavigationRequest` creation is paramount. For instance, the `url`, `referrer`, and `transition` type must be validated to prevent injection of malicious URLs or manipulation of navigation behavior.
-   **Origin and Site Isolation**: The `origin` and `SiteInfo` are determined early in the creation process. These parameters are fundamental for establishing and enforcing site isolation. Correctly setting these parameters from the outset is crucial for maintaining process separation and preventing cross-site scripting and other site isolation vulnerabilities.
-   **Security Policy Initialization**: Security policies such as CSP, COOP, and COEP are initialized and associated with the `NavigationRequest` during its creation. Proper initialization ensures that these policies are in place and ready to be enforced throughout the navigation, starting from the initial request.
-   **Creation Method Choice**: Choosing the appropriate creation method (`CreateBrowserInitiated`, `CreateRendererInitiated`, `CreateForSynchronousRendererCommit`) is vital for establishing the correct security context. Each method is designed for a specific navigation type and ensures that the `NavigationRequest` is set up with the appropriate security defaults and checks for that context.

Ensuring these security considerations are addressed during `NavigationRequest` creation is essential for building a secure navigation process in Chromium.

## Further Investigation

-   The detailed logic of each creation method and how they differ.
-   The usage of the various parameters in the creation methods.
-   The interaction between `NavigationRequest` creation and other components, such as `NavigationController` and `Navigator`.
-   The handling of special cases, such as browser-initiated vs. renderer-initiated navigations and synchronous renderer commits.

## Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_entry_impl.h`
-   `content/browser/renderer_host/navigation_entry_impl.cc`
-   `content/browser/renderer_host/navigation_controller_impl.h`
-   `content/browser/renderer_host/navigation_controller_impl.cc`
-   `content/browser/navigator.h`
-   `content/browser/navigator.cc`
