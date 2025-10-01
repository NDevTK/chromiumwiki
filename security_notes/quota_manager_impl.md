# QuotaManagerImpl (`storage/browser/quota/quota_manager_impl.h`)

## 1. Summary

The `QuotaManagerImpl` is the central authority in the browser process for managing disk space usage for all websites. It is a singleton per `StoragePartition` and is responsible for tracking usage, enforcing storage limits (quotas), and orchestrating the eviction of data when the system is under storage pressure.

This class is critical for both security and stability. From a stability perspective, it prevents any single website from consuming all available disk space, which would be a denial-of-service attack against the user's computer. From a security perspective, it is a key component in enforcing storage isolation between different origins and sites.

## 2. Core Concepts

*   **StorageKey Partitioning:** The fundamental security principle of the quota system is that all storage is partitioned by a `blink::StorageKey`. This key encapsulates not just the origin creating the data, but also the top-level site, which is crucial for preventing third-party iframes from accessing or exhausting the storage quota of the first-party site.

*   **Quota Clients:** The `QuotaManagerImpl` does not know the details of specific storage APIs (like IndexedDB, Cache Storage, etc.). Instead, it communicates with them through a generic `mojom::QuotaClient` interface. Each storage backend registers itself as a client and is responsible for reporting its usage and deleting data when requested by the `QuotaManagerImpl`.

*   **Buckets:** The modern storage standard organizes storage into "buckets." A single `StorageKey` can have multiple buckets (e.g., a default bucket, an "inbox" bucket). The `QuotaManagerImpl` is responsible for creating, managing metadata for (like expiration dates), and deleting these buckets. All quota calculations are performed at the `StorageKey` level, shared across all of its buckets.

*   **Usage Tracking and Quota Calculation:** The manager tracks usage for each `StorageKey` and `QuotaClientType`. It calculates the global quota based on the total disk size and available space (`GetVolumeInfo`) and applies a fraction of that as the per-origin limit.

*   **Eviction:** When available disk space is low, the `QuotaTemporaryStorageEvictor` is activated. It queries the `QuotaManagerImpl` for a list of the least recently used buckets and then instructs the relevant `QuotaClient`s to delete the data for those buckets, freeing up space.

## 3. Security-Critical Logic & Vulnerabilities

*   **Storage Isolation (Partitioning):**
    *   **Risk:** The most critical security risk is a flaw that could cause data from one `StorageKey` to be accounted for under another. This could lead to a variety of attacks. For example, if `evil.com` could get its storage usage charged to `bank.com`, it could fill `bank.com`'s quota, causing a denial of service. If it could trigger an eviction that incorrectly deletes data from `bank.com`, it could cause data loss.
    *   **Mitigation:** The entire system is architected around the `StorageKey` (or the `BucketLocator`, which contains it). Every IPC and internal method call uses this as the primary identifier. The security of the system depends on the strict and correct propagation and validation of this key at every step.

*   **Denial of Service:**
    *   **Risk:** A malicious website could attempt to write an infinite amount of data to disk, causing the browser and potentially the entire user system to fail.
    *   **Mitigation:** This is the core purpose of the `QuotaManagerImpl`. The enforcement of the global and per-origin quotas is the primary defense. A bug in the usage tracking (`UsageTracker`) or a failure in the eviction logic could weaken this defense and re-open the DoS vector.

*   **Information Leaks via Quota Queries:**
    *   **Risk:** The `getUsageAndQuota` API could potentially leak information. For example, if the returned quota value varied significantly based on the user's total disk space or browsing mode (e.g., incognito), it could be used as a fingerprinting vector.
    *   **Mitigation:** The `GetUsageAndReportedQuotaWithBreakdown` method is careful to return a static, standardized quota value for most sites, rather than the true, dynamically calculated value. The real quota is only revealed to sites that have been granted "unlimited storage" permission, which is a trusted state.

*   **Bypassing Eviction (`SpecialStoragePolicy`):**
    *   **Risk:** The `SpecialStoragePolicy` allows certain origins (like extensions or enterprise-configured sites) to be marked as non-evictable. If a normal, untrusted website could somehow trick the `QuotaManagerImpl` into thinking it has this special policy, it could make its data permanent and immune to storage pressure, effectively enabling a DoS attack.
    *   **Mitigation:** The `SpecialStoragePolicy` is configured by the browser process based on trusted inputs (e.g., extension manifests, enterprise policy) and is not controllable by web content.

## 4. Key Functions

*   `UpdateOrCreateBucket(...)`: The primary entry point for getting a handle to a storage bucket. It enforces policies like expiration.
*   `GetUsageAndQuota(...)` / `GetUsageAndReportedQuotaWithBreakdown(...)`: The core methods for querying usage and quota, containing the logic to prevent information leaks.
*   `NotifyBucketModified(...)`: The critical callback that all storage backends must use to report changes in their data usage.
*   `DeleteBucketData(...)`: The entry point for clearing all data associated with a bucket.
*   `StartEviction()`: Initiates the process of freeing up disk space when under pressure.

## 5. Related Files

*   `storage/browser/quota/quota_database.h`: The class that manages the persistent SQLite database for all bucket and quota metadata.
*   `storage/browser/quota/quota_temporary_storage_evictor.h`: Contains the LRU logic for selecting which buckets to evict.
*   `storage/browser/quota/usage_tracker.h`: Tracks usage per `StorageKey`.
*   `third_party/blink/public/common/storage_key/storage_key.h`: Defines the `StorageKey`, the fundamental security principal for storage partitioning.
*   `content/browser/storage_partition_impl.h`: The owner of the `QuotaManagerImpl` instance.