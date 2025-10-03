# Security Notes for `net/cookies/cookie_monster.cc`

The `CookieMonster` is the heart of Chromium's cookie storage system. It is the in-memory representation of the "cookie jar" and is responsible for all low-level cookie management, including storing, retrieving, and deleting cookies. It acts as the final gatekeeper, enforcing storage limits and security policies before a cookie is persisted or returned to a network request. It is the implementation of the `net::CookieStore` interface.

## Key Data Structures and Concepts

*   **`cookies_`**: A `std::multimap` that stores all unpartitioned cookies, keyed by their eTLD+1 domain (e.g., `google.com`). This is the main storage for traditional, third-party, and first-party cookies.
*   **`partitioned_cookies_`**: A map from a `CookiePartitionKey` to another `CookieMap`. This is the core data structure for implementing CHIPS (Cookies Having Independent Partitioned State). It ensures that a third-party's cookies set in the context of `siteA.com` are stored in a completely separate "jar" from its cookies set in the context of `siteB.com`.
*   **Persistent Store**: The `CookieMonster` is initialized with a `PersistentCookieStore` backend (usually an SQLite database), which it uses to load cookies on startup and to persist changes.
*   **Lazy Loading**: For performance, the `CookieMonster` does not load the entire cookie database at startup. Instead, it can load cookies on-demand for a specific eTLD+1 (`LoadCookiesForKey`) or load everything in the background (`FetchAllCookies`). This lazy loading model is a performance/security trade-off.

## Core Security Mechanisms

### 1. Eviction and Garbage Collection (Anti-DoS)

The `CookieMonster` implements a robust set of limits and eviction policies to prevent a single domain or an attacker from overwhelming the cookie jar, which would be a denial-of-service attack (e.g., evicting a user's login cookies).

*   **Hard Limits**: `kMaxCookies` (global limit, e.g., 3300) and `kDomainMaxCookies` (per-eTLD+1 limit, e.g., 180). There are also size-based limits for partitioned cookies (`kPerPartitionDomainMaxCookieBytes`).
*   **Eviction Strategy**: When a limit is exceeded, `GarbageCollect` is triggered. The eviction strategy is multi-layered and security-aware:
    1.  **Expired Cookies**: Always deleted first.
    2.  **Priority-Based**: Cookies are categorized as `LOW`, `MEDIUM`, or `HIGH` priority. The eviction algorithm purges lower-priority cookies before higher-priority ones.
    3.  **Least Recently Used (LRU)**: Within a priority level, cookies with the oldest `LastAccessDate` are purged first.
    4.  **Secure Preference**: Within a priority level, non-secure cookies are purged before secure cookies.
    5.  **Origin-Bound Preference**: When origin-bound cookies are enabled, domain-scoped cookies are purged before host-scoped cookies.
*   **Purpose**: This complex eviction logic is designed to preserve the most important cookies (high-priority, recently used, secure, host-scoped) during a storage pressure scenario.

### 2. Duplicate and Alias Handling

*   **`TrimDuplicateCookiesForKey`**: When loading from the store, this function ensures that if any "equivalent" cookies exist (same unique key), only the one with the most recent creation date is kept. This prevents database corruption or ambiguity from causing security issues.
*   **`DeleteAllAliasingCookies`**: This function is part of the security migration to origin-bound cookies. When a domain is first encountered in the new, stricter scope, this function is called to delete older, legacy-scoped cookies that could "alias" (i.e., have the same name/domain/path) with a new, more specific cookie. This is critical to prevent a less-specific cookie from being used where a more-specific one is expected.

### 3. Secure Cookie Overwrite Protection

*   **`MaybeDeleteEquivalentCookieAndUpdateStatus`**: This method implements the "Leave Secure Cookies Alone" RFC draft standard. It prevents an insecure request (e.g., from `http://`) from overwriting a cookie that has the `Secure` flag and is "equivalent" to the one being set. This is a crucial defense against an active network attacker trying to downgrade a session by injecting a non-secure session cookie.

### 4. Asynchronous Operations and Loading

*   The lazy-loading mechanism queues up cookie requests (`tasks_pending_`, `tasks_pending_for_key_`) until the necessary data has been loaded from the persistent store. While primarily for performance, this ensures that cookie operations are never performed on a partially-loaded view of the cookie jar for a given key, which could lead to inconsistent security decisions.

## Potential Attack Surface and Research Areas

*   **Eviction Logic Manipulation**: Could an attacker craft a set of cookies with specific priorities, access times, and security attributes to force the eviction of a targeted, high-value cookie from another origin within the same eTLD+1? This would be a sophisticated attack but is the primary threat model for the eviction logic.
*   **Partitioning Logic Flaws**: The correctness of the `partitioned_cookies_` map is paramount. Any bug that caused cookies from one partition to be stored in or read from another would break the CHIPS security guarantee. Fuzzing the `CookiePartitionKey` creation and lookup logic is a valuable area of research.
*   **Timing Side-Channels**:
    *   Does a request for `foo.com` take measurably longer if it's the first request for that domain in a session (triggering `LoadCookiesForKey`) versus a subsequent request? This could leak information about the user's browsing history to a network observer.
    *   The `SiteHasCookieInOtherPartition` function is a potential source of side-channel leaks. It checks if a site has set cookies under other partition keys. While this is used for UI/permission purposes, its timing could reveal information about a user's cross-site activity.
*   **Race Conditions**: The interaction between asynchronous loading (`OnLoaded`, `OnKeyLoaded`) and synchronous operations could potentially create race conditions. For example, could a `DeleteAll` operation race with a `LoadCookiesForKey` operation in a way that leads to an inconsistent state?

In summary, `CookieMonster` is the last line of defense for cookie storage. Its security hinges on the complex and multi-layered eviction logic, the correct implementation of cookie partitioning, and the careful handling of asynchronous data loading from the persistent store.