# RenderProcessHost Mojo Interfaces

This page details the Mojo interfaces that `RenderProcessHostImpl` binds and provides to the renderer process.

## Overview

`RenderProcessHostImpl` uses Mojo interfaces for communication with the renderer process. These interfaces are defined in `.mojom` files and are used to expose specific functionality to the renderer.

## Interface Binding

`RenderProcessHostImpl` binds various Mojo interfaces to provide services to the renderer process. Some of the key interfaces include:

-   `CacheStorage`: Provides access to the CacheStorage API.
-   `IndexedDB`: Provides access to the IndexedDB API.
-   `FileSystemManager`: Provides access to the File System API.
-   `FileSystemAccessManager`: Provides access to the File System Access API.
-   `FileBackedBlobFactory`: Creates file-backed blobs.
-   `RestrictedCookieManager`: Provides access to cookies for service workers.
-   `VideoDecodePerfHistory`: Provides access to video decode performance history.
-   `WebrtcVideoPerfHistory`: Provides access to WebRTC video performance history.
-   `QuotaManagerHost`: Provides access to the Quota Management API.
-   `LockManager`: Provides access to the Web Locks API.
-   `PermissionService`: Provides access to the Permissions API.
-   `PaymentManager`: Provides access to the Payment Request API.
-   `NotificationService`: Provides access to the Notifications API.
-   `WebSocketConnector`: Creates WebSocket connections.
-   `StableVideoDecoder`: Provides access to a stable video decoder.
-   `MediaInterfaceProxy`: Provides access to media-related interfaces.
-   `VideoEncoderMetricsProvider`: Provides video encoder metrics.
-   `AecDumpManager`: Manages AEC dumps.
-   `PluginRegistry`: Provides access to plugin information.
-   `DomStorage`: Provides access to DOM storage.
-   `RendererHost`: Provides access to renderer host functionality.
-   `CompositingModeReporter`: Reports compositing mode information.
-   `DomStorageProvider`: Provides access to DOM storage.
-   `MediaLogRecordHost`: Creates media log records.
-   `PushMessaging`: Provides access to the Push Messaging API.
-   `P2PSocketManager`: Manages P2P sockets.
-   `ChildHistogramFetcherFactory`: Creates child histogram fetchers.
-   `OneShotBackgroundSyncService`: Creates a one-shot background sync service.
-   `PeriodicBackgroundSyncService`: Creates a periodic background sync service.
-   `CoordinatorConnector`: Connects to the memory instrumentation coordinator.
-   `TracedProcess`: Provides tracing functionality.
-   `EmbeddedFrameSinkProvider`: Creates embedded frame sink providers.

## Interface Requests

The renderer process can request interfaces from the `RenderProcessHostImpl` using the `OnAssociatedInterfaceRequest` method.

## Further Investigation

-   The detailed implementation of each Mojo interface and the services they provide.
-   The security implications of exposing these interfaces to the renderer process.
-   The interaction between `RenderProcessHostImpl` and other components, such as `StoragePartitionImpl`, in providing these services.
-   The usage of these interfaces by different web platform features and APIs.

## Related Files

-   `content/browser/renderer_host/render_process_host_impl.h`
-   `content/browser/renderer_host/render_process_host_impl.cc`
-   Various `.mojom` files defining the interfaces.