# NavigationRequest

This page details the `NavigationRequest` class and its role in site isolation.

## Core Concepts

The `NavigationRequest` class manages the navigation lifecycle and determines the `UrlInfo` for the navigation. It plays a crucial role in enforcing site isolation by making decisions about process allocation and security checks. A `NavigationRequest` is created for each navigation, and it lives from the start of the navigation until the navigation commits, is replaced, or is canceled.

For a description of the core concepts, see [NavigationRequest Core](navigation_request_core.md).
For details on the navigation lifecycle, see [NavigationRequest Lifecycle](navigation_request_lifecycle.md).
For details on the security aspects, see [NavigationRequest and Security](navigation_request_security.md).
For details on the data associated with a `NavigationRequest`, see [NavigationRequest Data](navigation_request_data.md).
For details on how a `NavigationRequest` is created, see [NavigationRequest Creation](navigation_request_creation.md).

## Key Areas of Concern

-   Incorrectly determining the `UrlInfo` for the navigation can lead to incorrect isolation decisions.
-   Errors in handling redirects and cross-origin navigations can compromise security.
-   Incorrectly applying security policies during navigation, such as CSP, COOP, and COEP.
-   Potential issues with the interaction between navigation and other security mechanisms.
-   Incorrectly managing the state transitions during the navigation lifecycle can lead to unexpected behavior.
-   Errors in selecting the appropriate `RenderFrameHost` for the navigation can break site isolation.
-   Issues with handling the cancellation window for renderer-initiated navigations.
-   Incorrectly determining the origin to commit for the navigation.
-   Problems with handling special cases like data URLs, about:blank, and downloads.
-   Incorrectly managing `BrowsingInstance` swaps during navigation.
-   Security issues related to the use of `NavigationHandle` and its interaction with `NavigationRequest`.

### Security Considerations

-   A vulnerability existed where the Android address bar could be hidden after a slow navigation finishes if the slow navigation is initiated on page load. This could lead to potential spoofing attacks. This issue has been fixed. (VRP.txt)
    - Fixed in commit: 379652406

### Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `chromiumwiki/navigation_request_core.md`

### Functions and Methods

-   `NavigationRequest::CreateBrowserInitiated`: Creates a new `NavigationRequest` for browser-initiated navigations.
-   `NavigationRequest::CreateRendererInitiated`: Creates a new `NavigationRequest` for renderer-initiated navigations.
-   `NavigationRequest::CreateForSynchronousRendererCommit`: Creates a new `NavigationRequest` for synchronous navigations that have committed in the renderer process.
-   `NavigationRequest::From`: Retrieves the `NavigationRequest` from a `NavigationHandle`.
-   `NavigationRequest::NavigationTypeToReloadType`: Converts a `NavigationType` to a `ReloadType`.
-   `NavigationRequest::BeginNavigation`: Starts the navigation process.
-   `NavigationRequest::OnRequestRedirected`: Handles redirects during navigation.
-   `NavigationRequest::OnResponseStarted`: Handles the start of a response.
-   `NavigationRequest::OnRequestFailedInternal`: Handles failed requests.
-   `NavigationRequest::CommitNavigation`: Commits the navigation.
-   `NavigationRequest::SelectFrameHostForOnResponseStarted`: Selects the appropriate `RenderFrameHost` for the navigation after a response is received.
-   `NavigationRequest::SelectFrameHostForOnRequestFailedInternal`: Selects the appropriate `RenderFrameHost` for the navigation after a request has failed.
-   `NavigationRequest::GetOriginForURLLoaderFactoryBeforeResponse`: Determines the origin for the navigation before the response is received.
-   `NavigationRequest::GetOriginForURLLoaderFactoryAfterResponse`: Determines the origin for the navigation after the response is received.
-   `NavigationRequest::ComputeCrossOriginIsolationKey`: Computes the `CrossOriginIsolationKey` for the navigation.
-   `NavigationRequest::AddOriginAgentClusterStateIfNecessary`: Adds the origin to the list of origins that are isolated by the Origin-Agent-Cluster.
-   `NavigationRequest::DetermineOriginAgentClusterEndResult`: Determines the final result of the origin agent cluster isolation.
-   `NavigationRequest::CheckCSPEmbeddedEnforcement`: Checks if the Content Security Policy Embedded Enforcement is valid.
-   `NavigationRequest::CheckCredentialedSubresource`: Checks if the subresource request contains embedded credentials.
-   `NavigationRequest::CheckAboutSrcDoc`: Checks if the navigation is to an about:srcdoc URL.
-   `NavigationRequest::ShouldRequestSiteIsolationForCOOP`: Determines if a site should be isolated due to COOP.
-   `NavigationRequest::ComputeCrossOriginEmbedderPolicy`: Computes the Cross-Origin-Embedder-Policy.
-   `NavigationRequest::CheckResponseAdherenceToCoep`: Checks if the response adheres to the embedder's COEP.
-   `NavigationRequest::OnStartChecksComplete`: Called when the NavigationThrottles have been checked.
-   `NavigationRequest::OnRedirectChecksComplete`: Called when the redirect checks have been completed.
-   `NavigationRequest::OnFailureChecksComplete`: Called when the failure checks have been completed.
-   `NavigationRequest::OnWillProcessResponseChecksComplete`: Called when the response processing checks have been completed.
-   `NavigationRequest::OnWillCommitWithoutUrlLoaderChecksComplete`: Called when the checks for committing without a URL loader have been completed.
-   `NavigationRequest::OnCommitDeferringConditionChecksComplete`: Called when the commit deferring condition checks have been completed.
-   `NavigationRequest::CommitErrorPage`: Commits an error page.
-   `NavigationRequest::ReadyToCommitNavigation`: Called when the navigation is ready to be committed.
-   `NavigationRequest::OnCommitTimeout`: Called when the commit timeout expires.
-   `NavigationRequest::RenderProcessBlockedStateChanged`: Called when the blocked state of the renderer process changes.
-   `NavigationRequest::StopCommitTimeout`: Stops the commit timeout.
-   `NavigationRequest::RestartCommitTimeout`: Restarts the commit timeout.
-   `NavigationRequest::IsSelfReferentialURL`: Checks for attempts to navigate to a page that is already referenced more than once in the frame's ancestors.
-   `NavigationRequest::UpdateStateFollowingRedirect`: Updates the navigation handle state after encountering a server redirect.
-   `NavigationRequest::UpdatePrivateNetworkRequestPolicy`: Updates the private network request policy.
-   `NavigationRequest::RecordEarlyRenderFrameHostSwapMetrics`: Records metrics for early RenderFrameHost swaps.
-   `NavigationRequest::GetOriginForURLLoaderFactoryUncheckedWithDebugInfo`: Calculates the origin for the navigation without using information from RenderFrameHostImpl.
-   `NavigationRequest::GetOriginForURLLoaderFactoryUnchecked`: Same as above, but without debug info.
-   `NavigationRequest::ComputeCrossOriginIsolationKey`: Computes the CrossOriginIsolationKey for the navigation.
-   `NavigationRequest::ComputeWebExposedIsolationInfo`: Computes the web-exposed isolation information.
-   `NavigationRequest::ComputeCommonCoopOrigin`: Computes the common COOP origin for the navigation.
-   `NavigationRequest::MaybeAssignInvalidPrerenderFrameTreeNodeId`: Assigns an invalid frame tree node ID to `prerender_frame_tree_node_id_`.
-   `NavigationRequest::ComputeDownloadPolicy`: Computes the download policy for the navigation.
-   `NavigationRequest::PostResumeCommitTask`: Posts a task to resume a queued navigation.
-   `NavigationRequest::CheckSoftNavigationHeuristicsInvariants`: Checks invariants for soft navigation heuristics.
-   `NavigationRequest::UnblockPendingSubframeNavigationRequestsIfNeeded`: Resumes any deferred subframe history navigation requests.
-   `NavigationRequest::MaybeDispatchNavigateEventForCrossDocumentTraversal`: Sends a message to the renderer to fire the navigate event for same-origin cross-document traversals.
-   `NavigationRequest::ShouldAddCookieChangeListener`: Returns if a `CookieChangeListener` should be added for the current navigation.
-   `NavigationRequest::GetStoragePartitionWithCurrentSiteInfo`: Returns the `StoragePartition` based on the config from `site_info_`.
-   `NavigationRequest::OnResponseBodyReady`: Passes the response body contents to the original caller.
-   `NavigationRequest::RecordEarlyRenderFrameHostSwapMetrics`: Records metrics for early RenderFrameHost swaps.
-   `NavigationRequest::GetTentativeOriginAtRequestTime`: Calculates the origin that this NavigationRequest may commit.
-   `NavigationRequest::SetNavigationClient`: Sets the `NavigationClient` for this navigation.
-   `NavigationRequest::SetExpectedProcess`: Sets the expected process for this navigation.
-   `NavigationRequest::ResetExpectedProcess`: Resets the expected process.
-   `NavigationRequest::AddOldPageInfoToCommitParamsIfNeeded`: Adds information about the old page to the commit parameters if needed.
-   `NavigationRequest::RecordDownloadUseCountersPrePolicyCheck`: Records download-related UseCounters before the download policy check.
-   `NavigationRequest::RecordDownloadUseCountersPostPolicyCheck`: Records download-related UseCounters after the download policy check.
-   `NavigationRequest::RendererRequestedNavigationCancellationForTesting`: Simulates the renderer requesting navigation cancellation.
-   `NavigationRequest::RegisterCommitDeferringConditionForTesting`: Registers a commit deferring condition for testing.
-   `NavigationRequest::Resume`: Resumes a deferred navigation.
-   `NavigationRequest::CancelDeferredNavigation`: Cancels a deferred navigation.
-   `NavigationRequest::GetNavigationThrottleRunnerForTesting`: Returns the `NavigationThrottleRunner` for testing.
-   `NavigationRequest::DidCommitNavigation`: Called when the navigation was committed.
-   `NavigationRequest::RendererCancellationWindowEnded`: Called when the renderer cancellation window has ended.
-   `NavigationRequest::SetIsOverridingUserAgent`: Sets whether the user agent is being overridden.
-   `NavigationRequest::SetSilentlyIgnoreErrors`: Sets whether errors should be silently ignored.
-   `NavigationRequest::SetVisitedLinkSalt`: Sets the visited link salt.
-   `NavigationRequest::SetAllowCookiesFromBrowser`: Sets whether cookies are allowed from the browser.
-   `NavigationRequest::GetResponseBody`: Retrieves the response body.
-   `NavigationRequest::GetPrerenderTriggerType`: Returns the prerender trigger type.
-   `NavigationRequest::GetPrerenderEmbedderHistogramSuffix`: Returns the prerender embedder histogram suffix.
-   `NavigationRequest::set_force_new_browsing_instance`: Sets whether to force a new `BrowsingInstance`.
-   `NavigationRequest::force_new_browsing_instance`: Returns whether a new `BrowsingInstance` should be forced.
-   `NavigationRequest::set_force_new_compositor`: Sets whether to force a new compositor.
-   `NavigationRequest::force_new_compositor`: Returns whether a new compositor should be used.
-   `NavigationRequest::SetViewTransitionState`: Sets the view transition state.
-   `NavigationRequest::GetRuntimeFeatureStateContext`: Returns a const reference to the `RuntimeFeatureStateContext`.
-   `NavigationRequest::GetMutableRuntimeFeatureStateContext`: Returns a mutable reference to the `RuntimeFeatureStateContext`.
-   `NavigationRequest::set_pending_navigation_api_key`: Sets the pending navigation API key.
-   `NavigationRequest::GetNavigationTokenForDeferringSubframes`: Returns the navigation token for deferring subframes.
-   `NavigationRequest::set_main_frame_same_document_history_token`: Sets the main frame's same-document history token.
-   `NavigationRequest::main_frame_same_document_history_token`: Returns the main frame's same-document history token.
-   `NavigationRequest::AddDeferredSubframeNavigationThrottle`: Adds a deferred subframe navigation throttle.
-   `NavigationRequest::TakeCookieChangeListener`: Takes the cookie change listener.
-   `NavigationRequest::ShouldQueueDueToExistingPendingCommitRFH`: Returns true if the navigation should be queued due to an existing pending commit RFH.
-   `NavigationRequest::set_resume_commit_closure`: Sets the closure to resume the commit.
-   `NavigationRequest::RecordMetricsForBlockedGetFrameHostAttempt`: Records metrics for blocked `GetFrameHostForNavigation` attempts.
-   `NavigationRequest::CreateWebUIIfNeeded`: Creates a `WebUI` object if needed.
-   `NavigationRequest::HasWebUI`: Returns true if the navigation has a `WebUI`.
-   `NavigationRequest::web_ui`: Returns the `WebUIImpl` object.
-   `NavigationRequest::TakeWebUI`: Takes ownership of the `WebUIImpl` object.
-   `NavigationRequest::shared_storage_writable_eligible`: Returns whether the navigation is eligible for shared storage.
-   `NavigationRequest::ComputeErrorPageProcess`: Determines which process should handle an error page.
-   `NavigationRequest::set_early_render_frame_host_swap_type`: Sets the type of early RenderFrameHost swap.
-   `NavigationRequest::early_render_frame_host_swap_type`: Returns the type of early RenderFrameHost swap.
-   `NavigationRequest::set_previous_render_frame_host_id`: Sets the ID of the previous RenderFrameHost.
-   `NavigationRequest::IsWaitingForBeforeUnload`: Returns true if the navigation is waiting for the result of beforeunload.
-   `NavigationRequest::GetOriginalRequestURL`: Returns the original request URL.
-   `NavigationRequest::GetPreviousMainFrameURL`: Returns the previous main frame URL.
-   `NavigationRequest::IsServedFromBackForwardCache`: Returns true if the navigation is served from the back-forward cache.
-   `NavigationRequest::IsPageActivation`: Returns true if the navigation is activating an existing page.
-   `NavigationRequest::IsRestore`: Returns true if the navigation type is a restore navigation.
-   `NavigationRequest::IsReload`: Returns true if the navigation type is a reload navigation.
-   `NavigationRequest::SetPrerenderActivationNavigationState`: Sets state pertaining to prerender activations.
-   `NavigationRequest::TakePrerenderNavigationEntry`: Takes the prerender navigation entry.
-   `NavigationRequest::prerender_main_frame_replication_state`: Returns the frame replication state for prerender activations.
-   `NavigationRequest::AddOriginAgentClusterStateIfNecessary`: Adds the origin agent cluster state if necessary.
-   `NavigationRequest::AddDeferredConsoleMessage`: Adds a deferred console message.
-   `NavigationRequest::ShouldRenderFallbackContentForResponse`: Returns true if fallback content should be rendered for the response.

### Member Variables

-   `frame_tree_node_`: The `FrameTreeNode` associated with this navigation.
-   `is_synchronous_renderer_commit_`: Whether the navigation is a synchronous renderer commit.
-   `render_frame_host_`: The `RenderFrameHostImpl` that this navigation intends to commit in.
-   `common_params_`: Common parameters for the navigation.
-   `begin_params_`: Parameters for beginning the navigation.
-   `commit_params_`: Parameters for committing the navigation.
-   `same_origin_`: Whether the navigation is same-origin.
-   `browser_side_origin_to_commit_with_debug_info_`: The origin to commit, calculated at ReadyToCommit time, along with debug info.
-   `navigation_ui_data_`: The `NavigationUIData` for this navigation.
-   `blob_url_loader_factory_`: URLLoaderFactory for loading blob URLs.
-   `subresource_proxying_factory_bundle_`: URLLoaderFactory bundle for loading subresources.
-   `state_`: The current state of the navigation.
-   `is_navigation_started_`: Whether the navigation has started.
-   `service_worker_handle_`: Manages the lifetime of a pre-created ServiceWorkerContainerHost.
-   `loader_`: The `NavigationURLLoader` for this navigation.
-   `navigation_visible_to_embedder_`: Whether the navigation is visible to the embedder.
-   `source_site_instance_`: The source `SiteInstance`.
-   `dest_site_instance_`: The destination `SiteInstance`.
-   `restore_type_`: The restore type for the navigation.
-   `reload_type_`: The reload type for the navigation.
-   `nav_entry_id_`: The ID of the navigation entry.
-   `bindings_`: The bindings policy set.
-   `starting_site_instance_`: The starting `SiteInstance`.
-   `response_should_be_rendered_`: Whether the navigation response should be rendered in a renderer process.
-   `devtools_user_agent_override_`: Whether devtools overrides were applied on the User-Agent request header.
-   `devtools_accept_language_override_`: Whether devtools overrides were applied on the Accept-Language request header.
-   `associated_rfh_type_`: The type of `RenderFrameHost` associated with this navigation.
-   `speculative_site_instance_`: The speculative `SiteInstance` created on redirects.
-   `from_begin_navigation_`: Whether the `NavigationRequest` was created after receiving a BeginNavigation IPC.
-   `response_head_`: Holds response headers and metadata.
-   `response_body_`: Holds the response body.
-   `url_loader_client_endpoints_`: Holds the URLLoaderClientEndpoints.
-   `ssl_info_`: Holds SSL information.
-   `auth_challenge_info_`: Holds authentication challenge information.
-   `is_download_`: Whether the navigation is a download.
-   `request_id_`: The global request ID.
-   `early_hints_manager_`: Manages early hints.
-   `has_stale_copy_in_cache_`: Whether a stale copy of the response is in the cache.
-   `net_error_`: The network error code.
-   `extended_error_code_`: Extended error information.
-   `resolve_error_info_`: Detailed host resolution error information.
-   `expected_render_process_host_id_`: The ID of the `RenderProcessHost` that the navigation is expected to commit in.
-   `site_info_`: The `SiteInfo` of this navigation.
-   `on_start_checks_complete_closure_`: Test-only callback run when start checks are complete.
-   `subresource_loader_params_`: Parameters for subresource loaders.
-   `document_token_`: The `DocumentToken` for the newly-committed document in a cross-document navigation.
-   `devtools_navigation_token_`: The navigation token for tagging the navigation in devtools.
-   `subresource_overrides_`: Subresource overrides.
-   `request_navigation_client_`: The `NavigationClient` interface for renderer-initiated navigations.
-   `commit_navigation_client_`: The `NavigationClient` interface used to commit the navigation.
-   `upgrade_if_insecure_`: Whether to upgrade HTTP redirects to HTTPS.
-   `navigation_entry_offset_`: The offset of the new document in the history.
-   `throttle_runner_`: Manages the `NavigationThrottle`s associated with this navigation.
-   `commit_deferrer_`: Defers the commit until certain conditions are met.
-   `subframe_entry_committed_`: Whether the navigation changed which `NavigationEntry` is current.
-   `did_replace_entry_`: Whether the committed entry replaced the existing one.
-   `should_update_history_`: Whether to update the session history.
-   `previous_main_frame_url_`: The previous main frame URL.
-   `navigation_type_`: The type of navigation.
-   `redirect_chain_`: The chain of redirects.
-   `sanitized_referrer_`: The sanitized referrer.
-   `was_redirected_`: Whether the navigation was redirected.
-   `from_download_cross_origin_redirect_`: Whether the navigation was triggered by a cross-origin redirect following a download attempt.
-   `prefetched_signed_exchange_cache_`: Holds prefetched signed exchanges.
-   `navigation_handle_timing_`: Timing information for the navigation.
-   `ready_to_commit_time_`: The time the navigation was ready to commit.
-   `begin_navigation_time_`: The time `BeginNavigation()` was called.
-   `receive_response_time_`: The time `OnResponseStarted()` was called.
-   `first_fetch_start_time_`: The first `fetchStart` time.
-   `final_receive_headers_end_time_`: The time when the final headers were received.
-   `will_start_request_time_`: The time `WillStartRequest()` was called.
-   `is_same_process_`: Whether the navigation is same-process.
-   `post_commit_error_page_html_`: The HTML content of the error page for failed navigations.
-   `begin_navigation_callback_for_testing_`: Test-only callback run when `BeginNavigation()` is called.
-   `complete_callback_for_testing_`: Test-only callback run when all throttle checks are performed.
-   `ready_to_commit_callback_for_testing_`: Test-only callback run when ready to call `CommitNavigation`.
-   `navigation_id_`: Unique ID for this navigation.
-   `commit_timeout_timer_`: Timer for detecting long commit times.
-   `render_process_blocked_state_changed_subscription_`: Subscription for renderer process blocked state changes.
-   `request_headers_`: The headers used for the request.
-   `modified_request_headers_`: Headers modified during the navigation.
-   `cors_exempt_request_headers_`: CORS-exempt request headers.
-   `removed_request_headers_`: Headers removed during the redirect phase.
-   `rfh_restored_from_back_forward_cache_`: The `RenderFrameHost` being restored from the back-forward cache, if any.
-   `is_back_forward_cache_restore_`: Whether the navigation is a back-forward cache restore.
-   `frame_entry_item_sequence_number_`: The item sequence number from the `FrameNavigationEntry`.
-   `frame_entry_document_sequence_number_`: The document sequence number from the `FrameNavigationEntry`.
-   `isolation_info_`: The `IsolationInfo` for this navigation.
-   `previous_render_frame_host_id_`: The ID of the previous `RenderFrameHost`.
-   `current_render_frame_host_id_at_construction_`: The ID of the current `RenderFrameHost` at request creation time.
-   `initiator_frame_token_`: The frame token of the initiator document.
-   `initiator_process_id_`: The process ID of the initiator document.
-   `initiator_document_token_`: The `DocumentToken` of the initiator document.
-   `sandbox_flags_initiator_`: The sandbox flags of the navigation's initiator.
-   `was_opener_suppressed_`: Whether the navigation had its opener suppressed.
-   `pending_entry_ref_`: Reference to the pending entry.
-   `processing_navigation_throttle_`: Whether a `NavigationThrottle` is currently running.
-   `required_csp_`: The required CSP for this navigation.
-   `is_credentialless_`: Whether the document will be committed inside a credentialless iframe.
-   `policy_container_builder_`: Builder for the `PolicyContainerPolicies`.
-   `coep_reporter_`: Reporter for Cross-Origin Embedder Policy violations.
-   `loading_mem_tracker_`: Tracks peak GPU memory usage during loading.
-   `private_network_request_policy_`: The private network request policy.
-   `web_features_to_log_`: Web features used during the navigation.
-   `console_messages_`: Console messages to be printed on the target `RenderFrameHost`.
-   `is_pdf_`: Whether the navigation is for PDF content.
-   `is_embedder_initiated_fenced_frame_navigation_`: Whether the navigation is an embedder-initiated fenced frame navigation.
-   `is_deferred_on_fenced_frame_url_mapping_`: Whether the navigation is deferred on fenced frame URL mapping.
-   `fenced_frame_url_mapping_start_time_`: The start time of fenced frame URL mapping.
-   `prerender_frame_tree_node_id_`: The root frame tree node ID of the prerendered page.
-   `prerender_navigation_state_`: State pertaining to a prerender activation.
-   `fenced_frame_properties_`: The `FencedFrameProperties` for this navigation.
-   `embedder_shared_storage_context_`: The contextual string for shared storage.
-   `prerender_trigger_type_`: The type to trigger prerendering.
-   `prerender_embedder_histogram_suffix_`: The suffix for a prerender embedder histogram.
-   `force_new_browsing_instance_`: Whether to force a new `BrowsingInstance` for testing.
-   `force_new_compositor_`: Whether to use a new compositor.
-   `navigation_or_document_handle_`: The `NavigationOrDocumentHandle` for this navigation.
-   `browser_side_origin_to_commit_with_debug_info_`: The origin to commit, calculated on the browser side, along with debug info.
-   `view_transition_resources_`: Scoped reference to view transition resources.
-   `runtime_feature_state_context_`: Context for Blink Runtime-Enabled Features.
-   `browsing_context_group_swap_`: Describes the reason for a `BrowsingContextGroup` swap.
-   `pending_navigation_api_key_`: Key for the Navigation API.
-   `subframe_history_navigation_throttles_`: `SubframeHistoryNavigationThrottle`s for this navigation.
-   `main_frame_same_document_navigation_token_`: Token for the main frame's same-document navigation.
-   `cookie_change_listener_`: Listener for cookie change events.
-   `lcpp_hint_`: LCP Critical Path Predictor managed hint data.
-   `web_ui_`: The `WebUIImpl` object for this navigation.
-   `shared_storage_writable_eligible_`: Whether the navigation is eligible for shared storage.
-   `early_render_frame_host_swap_type_`: The reason for an early RenderFrameHost swap.
-   `did_encounter_cross_origin_redirect_`: Whether the navigation encountered a cross-origin redirect.
-   `was_initiated_by_animated_transition_`: Whether the navigation was initiated by an animated transition.
-   `navigation_discard_reason_`: The reason for cancelling/discarding the navigation.
-   `force_no_https_upgrade_`: Whether to disable HTTPS upgrades for this navigation.
-   `request_method_`: The initial request method.
-   `was_reset_for_cross_document_restart_`: Whether the navigation was reset for a cross-document restart.
-   `weak_factory_`: Weak pointer factory for this object.
