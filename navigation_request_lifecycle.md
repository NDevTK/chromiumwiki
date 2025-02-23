# NavigationRequest Lifecycle

This page details the lifecycle of a `NavigationRequest` and its interaction with `NavigationThrottles` and `CommitDeferringConditions`.

## Navigation States

A `NavigationRequest` goes through various states during its lifecycle:

-   `NOT_STARTED`: Initial state.
-   `WAITING_FOR_RENDERER_RESPONSE`: Waiting for a BeginNavigation IPC from the renderer in a browser-initiated navigation.
-   `WILL_START_NAVIGATION`: Temporary state before unload handlers have run.
-   `WILL_START_REQUEST`: The navigation is visible to embedders. `NavigationThrottles` run the WillStartRequest event.
-   `WILL_REDIRECT_REQUEST`: The request is being redirected. `NavigationThrottles` run the WillRedirectRequest event.
-   `WILL_PROCESS_RESPONSE`: The response is being processed. `NavigationThrottles` run the WillProcessResponse event.
-   `WILL_COMMIT_WITHOUT_URL_LOADER`: The navigation does not require a request/response. `NavigationThrottles` run the WillCommitWithoutUrlLoader event.
-   `READY_TO_COMMIT`: The browser process has asked the renderer to commit the response.
-   `DID_COMMIT`: The response has been committed.
-   `CANCELING`: The request is being canceled.
     This state is entered when a `NavigationThrottle` cancels the navigation, or when a critical error occurs during the navigation process.
     -   **Entry points**: `CANCELING` state is entered from various states like `WILL_START_REQUEST`, `WILL_REDIRECT_REQUEST`, `WILL_PROCESS_RESPONSE`, `WILL_COMMIT_WITHOUT_URL_LOADER`, and `WILL_FAIL_REQUEST`.
     -   **During this state**: In the `CANCELING` state, the `NavigationRequest` starts the cancellation process. This involves calling `OnRequestFailedInternal` to handle the failure and potentially display an error page. It also ensures that pending subframe history navigations are canceled.
     -   **Transitions from `CANCELING`**: The `NavigationRequest` can transition from `CANCELING` to `READY_TO_COMMIT` (in some complex scenarios), `WILL_FAIL_REQUEST` (if failure handling needs to be re-evaluated), or remain in `CANCELING` until destruction.
-   `WILL_FAIL_REQUEST`: The request is failing. `NavigationThrottles` run the WillFailRequest event.
-   `DID_COMMIT_ERROR_PAGE`: The request failed with a net error code and an error page should be displayed.

## State Transitions

The `NavigationRequest` transitions between these states based on various events and conditions. The `SetState` method is used to update the state, and it includes a `DCHECK` to ensure that the state transition is valid.

## Navigation Throttles

`NavigationThrottles` can intercept the navigation at different stages (WillStartRequest, WillRedirectRequest, WillProcessResponse, WillFailRequest) and can defer, cancel, or proceed with the navigation. The `NavigationThrottleRunner` manages the execution of these throttles.

## Commit Deferring Conditions

`CommitDeferringConditions` can defer the commit of a navigation until certain conditions are met. The `CommitDeferringConditionRunner` manages the execution of these conditions.

## Further Investigation

-   The detailed logic of each state transition.
-   The interaction between `NavigationRequest`, `NavigationThrottles`, and `CommitDeferringConditions`.
-   The handling of edge cases and error conditions during the navigation lifecycle.
-   The role of the `NavigationHandle` in managing the navigation lifecycle.

## Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_throttle.h`
-   `content/browser/renderer_host/navigation_throttle.cc`
-   `content/browser/renderer_host/commit_deferring_condition.h`
-   `content/browser/renderer_host/commit_deferring_condition.cc`
