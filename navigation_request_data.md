# NavigationRequest Data

This page details the data associated with a `NavigationRequest` and how it is used in making decisions about site isolation and process allocation.

## Key Data Structures

-   **`UrlInfo`**: Contains the URL being navigated to, along with additional information such as the origin, whether the navigation is same-site or cross-site, and whether the navigation is renderer-initiated.
-   **`CommonNavigationParams`**: Common parameters for the navigation, such as the URL, referrer, transition type, and whether the navigation has a user gesture.
-   **`BeginNavigationParams`**: Parameters for beginning the navigation, such as request headers and load flags.
-   **`CommitNavigationParams`**: Parameters for committing the navigation, such as the response headers, the origin to commit, and the CSP.
-   **`SiteInfo`**: Represents the site of a URL and is used to determine if two URLs belong to the same site.
-   **`IsolationContext`**: Provides context for making process allocation decisions, such as the `BrowsingInstanceId` and whether the navigation is for a guest or fenced frame.
-   **`CrossOriginIsolationKey`**: Used to determine the isolation requirements for a navigation based on COOP, COEP, and CORP.

## Data Flow and Usage

The data associated with a `NavigationRequest` flows through various stages of the navigation process and is used to make decisions about site isolation, process allocation, and security checks.

1. **Navigation Start**: When a navigation starts, the `NavigationRequest` is created with initial data from the `CommonNavigationParams`, `BeginNavigationParams`, and other sources.
2. **`UrlInfo` Determination**: The `UrlInfo` for the navigation is determined based on the URL and other parameters. This information is used to make decisions about site isolation and process allocation.
3. **`SiteInfo` and `IsolationContext`**: The `SiteInfo` and `IsolationContext` are used to determine which process the navigation should commit in.
4. **Redirects**: If the navigation encounters redirects, the `UrlInfo` and other parameters may be updated.
5. **Response Handling**: When a response is received, the `NavigationRequest` processes the response headers and updates the navigation state accordingly.
6. **Security Checks**: Various security checks are performed, such as CSP, COOP, and COEP checks. The results of these checks may affect the navigation outcome.
7. **Commit**: Once the navigation is ready to commit, the `NavigationRequest` provides the necessary data to the `RenderFrameHost` for the commit to proceed.

## Further Investigation

-   The detailed logic for determining the `UrlInfo` for different types of navigations.
-   How the `UrlInfo`, `SiteInfo`, and `IsolationContext` are used to make process allocation decisions.
-   The impact of incorrect data on site isolation and security.
-   The flow of data between `NavigationRequest` and other components, such as `NavigationHandle`, `NavigationURLLoader`, and `RenderFrameHost`.

## Data Handling in `OnRequestRedirected`

The `OnRequestRedirected` method plays a key role in updating and maintaining the data structures associated with `NavigationRequest` during redirects. It specifically updates:

-   **`UrlInfo`**: While not directly modified, the `UrlInfo` is re-evaluated based on the updated URL after the redirect, ensuring that site isolation and process allocation decisions are based on the new URL.
-   **`CommonNavigationParams`**: This structure is updated with new information from the redirect, such as:
    -   `url`: Updated to the redirect URL (`redirect_info.new_url`).
    -   `method`: Updated to the redirect method (`redirect_info.new_method`).
    -   `referrer`: Updated and sanitized based on the redirect URL and policy.
    -   `redirect_chain_`: The redirect URL is added to the redirect chain to keep track of the navigation history.

By updating these data structures, `OnRequestRedirected` ensures that the `NavigationRequest` maintains an accurate and consistent state throughout the redirect process, which is crucial for correct security policy enforcement and process management.

## Related Files

-   `content/browser/renderer_host/navigation_request.h`
-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/url_info.h`
-   `content/browser/url_info.cc`
-   `content/browser/site_info.h`
-   `content/browser/site_info.cc`
-   `content/browser/isolation_context.h`
