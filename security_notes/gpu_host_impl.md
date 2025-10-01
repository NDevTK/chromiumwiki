# GpuHostImpl (`components/viz/host/gpu_host_impl.h`)

## 1. Summary

The `GpuHostImpl` is a singleton class that represents the browser process's sole point of contact with the sandboxed GPU process. It is the "host" side of the GPU service and is responsible for launching, managing, and terminating the GPU process, as well as brokering all communication to and from it.

This class is a critical security boundary. It acts as the gatekeeper for all GPU capabilities, deciding which clients (e.g., renderer processes, the browser UI compositor) are allowed to establish a channel to the GPU and what operations they can perform. A vulnerability in `GpuHostImpl` could allow a compromised renderer to escalate its privileges by gaining unauthorized access to the GPU, leading to a sandbox escape, information disclosure, or denial of service.

## 2. Core Concepts

*   **GPU Process Management:** `GpuHostImpl` manages the entire lifecycle of the GPU process. It sends the initialization parameters (`InitParams`), receives the results (`DidInitialize`, `DidFailInitialize`), and handles crashes (`OnProcessCrashed`).

*   **Mojo Interface Brokering:** It exposes the primary `viz.mojom.GpuService` Mojo interface. All other clients who wish to talk to the GPU process must go through the `GpuHostImpl`.

*   **Channel Management:** Its most important function is managing "channels" to the GPU process. A channel is a dedicated Mojo message pipe for a specific client (e.g., a renderer).
    *   A client calls `EstablishGpuChannel`.
    *   The `GpuHostImpl` forwards this request to the `GpuService` in the GPU process.
    *   The `GpuService` creates the channel and returns a handle to the `GpuHostImpl`.
    *   The `GpuHostImpl` then passes this handle back to the original client.
    This brokering ensures that no client can create a channel without the browser process's knowledge and approval.

*   **Delegate Pattern:** The class uses a `Delegate` interface to communicate with its owner (typically in `//content`). This allows the embedder (e.g., Chrome) to implement product-specific policies, such as:
    *   Deciding if GPU access is allowed at all (`GpuAccessAllowed`).
    *   Handling GPU crashes by blocking domains (`BlockDomainsFrom3DAPIs`).
    *   Disabling GPU compositing entirely as a fallback (`DisableGpuCompositing`).

## 3. Security-Critical Logic & Vulnerabilities

*   **GPU Process Launch:** The `GpuHostImpl` is responsible for setting up the GPU process sandbox. Any weakness in the sandbox policy it requests could compromise the entire security model.

*   **Channel Brokering and Validation:**
    *   **Risk:** The `EstablishGpuChannel` flow is a critical security checkpoint. A flaw that allows a client to establish a channel with incorrect or spoofed identifiers (`client_id`, `client_tracing_id`) could lead to one renderer impersonating another, potentially allowing it to access or manipulate another renderer's GPU resources.
    *   **Mitigation:** The `client_id` is generated in the browser process and is used to track all subsequent operations for that channel. The GPU process must treat this ID as the source of truth for ownership of GPU resources.

*   **3D API Blocking (Domain Guilt):**
    *   **Risk:** A GPU driver bug can be a source of instability or security vulnerabilities. The `DidLoseContext` method is a critical signal that something has gone wrong in the GPU process.
    *   **Mitigation:** When a context is lost, the `GpuHostImpl` notifies its `Delegate`, which then calls `BlockDomainsFrom3DAPIs`. This mechanism adds the URL that was active at the time of the crash to a blocklist, preventing it from using 3D APIs (like WebGL) in the future. This quarantines potentially malicious web content that is intentionally trying to trigger a driver bug to exploit the system. A failure to correctly identify the guilty URL or to enforce the block would negate this protection.

*   **Information Leaks:** The `GpuHostImpl` is the conduit for all GPU-related information back to the browser (e.g., `GPUInfo`, `GpuFeatureInfo`). A bug that caused it to leak more information than necessary, or to leak information to the wrong process, could provide an attacker with valuable fingerprinting data to aid in exploitation.

*   **Interface Binding:** The `BindInterface` method allows the host to bind arbitrary interfaces on behalf of clients. This is a powerful capability that must be used with care. If a renderer could trick the `GpuHostImpl` into binding a privileged, internal-only interface on its behalf, it could lead to a sandbox escape. Security relies on the `Delegate` correctly validating which interfaces are allowed to be requested.

## 4. Key Functions

*   `EstablishGpuChannel(...)`: The central method for brokering a new communication channel to the GPU process for a client.
*   `DidLoseContext(...)`: The handler for GPU context loss, which triggers the domain-blocking security mechanism.
*   `BlockDomainsFrom3DAPIs(...)` (via Delegate): The action taken to quarantine a potentially malicious origin after a GPU crash.
*   `gpu_service()`: Provides the raw Mojo `Remote` to the `GpuService`, which is the root interface for all operations in the GPU process.

## 5. Related Files

*   `services/viz/privileged/mojom/gl/gpu_service.mojom`: The Mojo interface definition for the service running in the GPU process. `GpuHostImpl` is the client for this interface.
*   `content/browser/gpu/gpu_process_host.h`: The class in `//content` that is responsible for actually launching and managing the GPU OS process. It owns the `GpuHostImpl`.
*   `components/viz/service/gl/gpu_service_impl.h`: The server-side implementation of the `GpuService` interface, which runs in the sandboxed GPU process. This is the code that receives and acts upon the requests sent by `GpuHostImpl`.