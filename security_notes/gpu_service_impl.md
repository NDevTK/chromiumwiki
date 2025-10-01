# GpuServiceImpl (`components/viz/service/gl/gpu_service_impl.h`)

## 1. Summary

The `GpuServiceImpl` is the main entry point and "root" object of the sandboxed GPU process. It is the server-side implementation of the `viz.mojom.GpuService` Mojo interface, making it the direct counterpart to the `GpuHostImpl` that lives in the privileged browser process.

This class receives and orchestrates all requests from the browser process, such as creating communication channels for renderers, managing GPU memory, and initializing graphics contexts (OpenGL, Vulkan, Dawn, Metal). Its primary security role is to perform these privileged graphics operations safely within the confines of the GPU sandbox, acting as the final gatekeeper before commands are sent to the underlying system graphics drivers.

## 2. Core Concepts

*   **Sandboxed Service:** `GpuServiceImpl` is designed to run in a dedicated, sandboxed process with minimal privileges. It cannot access the filesystem (except for specific cache directories for which it is passed handles), create network connections, or access most OS-level APIs directly. All such operations must be brokered through its `mojom::GpuHost` remote connection back to the browser process.

*   **Channel Management:** Its most important responsibility is managing `GpuChannel`s via the `GpuChannelManager`. A `GpuChannel` is a dedicated IPC pipe for a single client (like a renderer process) to send its stream of GPU commands. The `GpuServiceImpl` creates these channels via `EstablishGpuChannel` at the request of the `GpuHostImpl`, ensuring that the browser process authorizes every new client.

*   **Resource and Context Management:** This class owns and manages the lifecycle of all core GPU resources, including:
    *   `GpuChannelManager`: Manages all client channels.
    *   `SharedImageManager`: Manages shared memory buffers that can be used as textures across different processes.
    *   `SyncPointManager`: Provides a synchronization mechanism between the CPU and GPU across different channels.
    *   Context Providers (Vulkan, Metal, Dawn): Manages the native graphics contexts.

## 3. Security-Critical Logic & Vulnerabilities

The security of the entire GPU architecture relies on the `GpuServiceImpl` correctly handling requests within its sandbox.

*   **GPU Driver Exploitation:** This is the most significant threat. The `GpuServiceImpl` and its sub-components (like the command buffer decoder) are the last line of defense before GPU commands are passed to the kernel-mode graphics driver. A bug in a driver can lead to a full system compromise. If a compromised renderer can craft a malicious stream of GPU commands (e.g., a malformed shader, an invalid texture state) and the `GpuServiceImpl` fails to validate it, it could be sent to the driver, triggering the vulnerability and escaping the sandbox.

*   **Information Leaks (Cross-Origin):** The GPU process is a multi-tenant environment, handling textures and data from many different origins at once. A critical security requirement is to prevent one origin from reading another's pixel data.
    *   **Risk:** A bug in the `SharedImageManager` or the command buffer validation could allow one renderer to guess or forge the identifier (e.g., a `Mailbox`) for a texture created by another renderer, allowing it to read the contents of that texture.
    *   **Mitigation:** The system relies on unguessable tokens and strict validation of resource IDs against the `client_id` of the channel making the request. The `GetIsolationKey` method is also used for WebGPU to ensure resources are partitioned by the requesting origin's security context.

*   **Sandbox Integrity:** The `GpuServiceImpl` must *never* attempt to perform a privileged operation for which it has not been explicitly passed a capability (e.g., a handle) from the browser process. For example, when it writes to the shader cache, it uses file handles provided by the browser via `SetChannelPersistentCacheFile`. A bug that allowed it to open an arbitrary file path would be a sandbox escape.

*   **Denial of Service:** A malicious renderer could try to exhaust GPU resources by creating too many channels, allocating massive textures, or submitting infinitely looping shaders. The `GpuChannelManager` and the `GpuWatchdogThread` are responsible for enforcing resource limits and terminating runaway GPU tasks to mitigate DoS attacks. The `MaybeExitOnContextLost` method is a final, drastic mitigation that can terminate the entire GPU process if it enters a bad state.

## 4. Key Functions

*   `InitializeWithHost(...)`: The main entry point, called by the browser process to start the service and establish the connection back to the `GpuHost`.
*   `EstablishGpuChannel(...)`: The handler for a browser request to create a new communication channel for a client. This is a primary IPC entry point.
*   `DidLoseContext(...)`: A critical security signal. When a client's GPU context is lost, this method is called. It reports the failure back to the `GpuHost`, which can then use this information for domain-guilt and other security heuristics.
*   `MaybeExitOnContextLost(...)`: A "fail-safe" mechanism. If a context loss is deemed unrecoverable, this function can terminate the entire GPU process to prevent further damage or exploitation.

## 5. Related Files

*   `components/viz/host/gpu_host_impl.h`: The client of this service, running in the privileged browser process.
*   `gpu/ipc/service/gpu_channel.h`: Represents a single client's communication channel to the GPU.
*   `gpu/ipc/service/gpu_channel_manager.h`: Manages the collection of all active `GpuChannel`s.
*   `gpu/command_buffer/service/decoder.h`: The interface for the command buffer decoder, which is the component responsible for validating the stream of GPU commands from a client before execution. This is a primary defense against driver exploitation.