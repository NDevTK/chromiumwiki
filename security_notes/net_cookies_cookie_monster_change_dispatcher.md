# Security Analysis of `net/cookies/cookie_monster_change_dispatcher.cc`

## Summary

The `CookieMonsterChangeDispatcher` is a critical component in Chromium's cookie management system. While `CookieMonster` handles the storage and retrieval of cookies, the change dispatcher is responsible for notifying other parts of the browser whenever a cookie is added, deleted, or modified. This notification system is essential for features like extensions, DevTools, and synchronization to maintain a consistent state.

From a security perspective, the dispatcher's primary role is to act as a gatekeeper, preventing information leakage. It must ensure that a listener (a "subscriber") only receives notifications about cookies it would be allowed to access according to the Same-Origin Policy (SOP) and other cookie-related security rules.

## The Core Security Mechanism: Reusing Access Control Logic

The most important security feature of the `CookieMonsterChangeDispatcher` is that it **does not invent its own security rules**. Instead, it reuses the canonical cookie access control logic from `CanonicalCookie::IncludeForRequestURL`.

When a change occurs, before notifying a specific subscriber, the dispatcher performs a check:

```cpp
// From CookieMonsterChangeDispatcher::Subscription::DispatchChange

if (!cookie.IncludeForRequestURL(url_, ...).status.IsInclude()) {
  return; // Do not dispatch the notification
}
```

This is the cornerstone of the dispatcher's security. It asks the question: "If the component that subscribed for notifications from `url_` were to make a request, would it be allowed to see this cookie?" This check implicitly enforces all the standard cookie security rules:

*   **Domain and Path Matching**: Ensures the subscriber's URL has the appropriate domain and path scope to access the cookie.
*   **`HttpOnly` Flag**: Prevents subscribers that represent script (like a web page's JavaScript context) from learning about `HttpOnly` cookie changes.
*   **`Secure` Flag**: Ensures that a subscriber in an insecure context (`http://`) cannot learn about cookies marked with the `Secure` flag.
*   **Same-Site Rules**: Enforces `SameSite` policies (`Lax`, `Strict`, `None`).

By reusing the core access control logic, the dispatcher avoids the risk of creating a dangerous side-channel. Without this check, a malicious extension or renderer could subscribe to cookie changes for one domain and use the notifications to spy on cookies from another, completely bypassing the SOP.

## Handling Partitioned Cookies (CHIPS)

The dispatcher also correctly handles the security boundaries for partitioned cookies (CHIPS). When a subscription is created, it can be associated with a specific `CookiePartitionKey`. Before dispatching a change for a partitioned cookie, the dispatcher verifies that the cookie's partition key matches the key the subscriber is listening for.

This prevents a site embedded in an iframe from learning about cookie changes that are happening in a different top-level site's partition, which is essential for the privacy and security goals of CHIPS.

## Subscription Scopes and Keys

The dispatcher uses a mapping system to efficiently find the right subscribers. It keys subscriptions by the cookie's eTLD+1 (using `GetDomainAndRegistry`) and its name.

*   **Domain Keying**: This is an optimization. While listeners are keyed by the registrable domain, the final security decision is always made using the full origin URL in the `IncludeForRequestURL` check, preserving the correct security boundary.
*   **Global Listeners**: The dispatcher supports "global" listeners that can receive notifications for all cookie changes. These are extremely powerful and are only intended for trusted, internal browser components like the sync engine. The security of this feature relies on higher-level code ensuring that only privileged components can register for these global notifications.

## Conclusion

The `CookieMonsterChangeDispatcher` is a security-critical component that prevents cookie data from being leaked across security boundaries via its notification mechanism. Its strength comes from its disciplined reuse of the browser's core cookie access control logic (`IncludeForRequestURL`), ensuring that the rules for being *notified* about a cookie are the same as the rules for *reading* it. This elegant design prevents the notification system from becoming a side-channel that could undermine the Same-Origin Policy.