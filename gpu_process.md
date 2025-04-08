# Component: GPU Process

## 1. Component Focus
*   **Functionality:** A dedicated, sandboxed process responsible for handling GPU-accelerated tasks on behalf of the browser and renderer processes. This includes graphics rendering (WebGL, WebGPU, compositor), video decoding/encoding (hardware acceleration via WebCodecs), and potentially other GPU computations. Isolating GPU operations helps stability and security (limits impact of driver bugs or complex graphics API exploits).
*   **Key Logic:** Process launching and sandboxing (`GpuProcessHost`), IPC/Mojo communication for receiving commands and data (`GpuChannelManager`, various Mojo interfaces like `viz.mojom.GpuService`, `media.mojom.VideoDecoder`, `gpu.mojom.WebGPU`), interaction with graphics drivers via platform APIs (OpenGL, Vulkan, Metal, Direct3D) or abstraction layers like ANGLE/Dawn, managing GPU memory and resources.
*   **Core Files:**
    *   `content/browser/gpu/`: Browser-side code for managing the GPU process (e.g., `gpu_process_host.cc`, `gpu_data_manager_impl.cc`).
    *   `content/gpu/`: Code running *within* the GPU process (e.g., `gpu_service_factory.cc`).
    *   `gpu/`: Core GPU abstraction layer.
        *   `gpu/ipc/`: IPC/Mojo definitions and implementation for GPU communication.
        *   `gpu/command_buffer/`: Implementation of the command buffer system used by WebGL and others.
        *   `gpu/config/`: GPU feature detection and configuration.
    *   `services/viz/`: Viz compositor service, often runs in the GPU process.
    *   `services/webgpu/`: WebGPU implementation running in the GPU process.
    *   `media/gpu/`: Hardware-accelerated video codec implementations.
    *   `ui/gl/`: OpenGL/ANGLE integration.
    *   `third_party/dawn/`: Runs within the GPU process for WebGPU.

## 2. Potential Logic Flaws & VRP Relevance
*   **Sandbox Escape via IPC/Mojo:** Exploiting vulnerabilities in the handling of IPC/Mojo messages received by the GPU process from less privileged processes (typically renderer) to execute arbitrary code within the GPU process sandbox, or potentially use GPU process capabilities to attack the browser process or OS.
    *   **VRP Pattern Concerns:** Missing validation on parameters for complex graphics/compute commands, memory corruption in command buffer deserialization/processing (`gpu/command_buffer/service/`), type confusion in Mojo interfaces, insecure handling of shared memory or file descriptors used for GPU communication. (Historical GPU bugs often fit this pattern).
*   **Memory Safety in GPU Process Code:** Vulnerabilities (UAF, buffer overflows, etc.) within the C++ code running in the GPU process, particularly in graphics API wrappers (ANGLE, Dawn), command buffer implementation, video codecs (`media/gpu/`), or shader compilers (Tint for WebGPU). These could potentially be triggered by crafted web content via WebGL/WebGPU/WebCodecs.
*   **Driver Vulnerabilities:** Exploiting bugs in the underlying OS graphics drivers triggered via WebGL/WebGPU calls passed through the GPU process. While technically driver bugs, Chrome aims to mitigate their impact via the sandbox and by validating/sanitizing API usage where possible.
*   **Information Leaks:** The GPU process potentially leaking sensitive data (e.g., uninitialized memory from GPU buffers, pixel data from other origins' textures, precise timing information) back to the renderer process.
*   **Denial of Service:** Causing GPU process crashes, hangs, or excessive resource consumption via malformed API calls, infinite shader loops, or resource allocation requests.

## 3. Further Analysis and Potential Issues
*   **IPC Boundary Validation:** Thoroughly audit all Mojo interfaces exposed by the GPU process (e.g., `viz.mojom.GpuService`, `media.mojom.*`, `gpu.mojom.WebGPU`) that are callable from the renderer. Ensure rigorous validation of all received data, command parameters, resource IDs, shared memory regions, etc.
*   **Command Buffer Security (`gpu/command_buffer/`):** Analyze the command buffer decoder logic for memory safety, correct state tracking, and validation of commands received from the client (renderer).
*   **Dawn/ANGLE Security:** Review the Dawn and ANGLE libraries (used for WebGPU and WebGL respectively) for vulnerabilities, particularly in how they handle API calls and interact with native drivers.
*   **Video Codec Security (`media/gpu/`):** Analyze hardware-accelerated video decoding/encoding implementations for memory safety issues when processing potentially malicious video streams via WebCodecs.
*   **Sandbox Policy:** Review the GPU process sandbox policy on different platforms. Does it effectively restrict access to unnecessary OS resources? Can it be bypassed?
*   **Resource Management & Isolation:** How does the GPU process manage and isolate resources (textures, buffers) requested by different renderer processes/origins?

## 4. Code Analysis
*   `GpuProcessHost`: Browser-side object responsible for launching and managing the GPU process.
*   `GpuChannelManager`: Manages communication channels between browser/renderers and the GPU process.
*   `GpuServiceImpl` (`services/viz/privileged/gpu_host/gpu_service_impl.cc`?): Browser-side endpoint for the main `viz.mojom.GpuService`.
*   `GpuCommandBufferStub`: Browser-side stub for command buffer channels.
*   `CommandBufferService` / `DecoderContext` (`gpu/command_buffer/service/`): GPU-process side implementation for decoding and executing command buffers. **Critical area for memory safety.**
*   `WebGpuServiceImpl` (`services/webgpu/`): GPU-process implementation for WebGPU, interacts with Dawn.
*   Video decoder/encoder implementations in `media/gpu/`.
*   Dawn (`third_party/dawn/`) / ANGLE (`third_party/angle/`): Translation layers to native graphics APIs.

## 5. Areas Requiring Further Investigation
*   **Fuzzing:** Fuzz Mojo interfaces exposed to renderers, the command buffer decoder, WGSL/GLSL shader compilers, and potentially Dawn/ANGLE entry points.
*   **IPC Validation Review:** Manual audit of validation logic in GPU process Mojo interface implementations.
*   **Memory Safety Analysis:** Use ASan/TSan/MSan during fuzzing and testing of GPU-related code. Focus on command buffer processing, video decoding, and Dawn/ANGLE.
*   **Driver Interaction:** Understand how Dawn/ANGLE interact with specific drivers and look for potential hazardous API usage patterns.
*   **Sandbox Effectiveness:** Review and test the GPU sandbox policy on different platforms.

## 6. Related VRP Reports
*   *(Numerous historical VRPs relate to GPU process sandbox escapes and memory corruption, often found via fuzzing. Specific IDs for WebGPU/WebCodecs related issues are less common currently but expected to increase).*

*(See also [webgpu.md](webgpu.md), [webcodecs.md](webcodecs.md), [webgl.md](webgl.md)?, [ipc.md](ipc.md), [mojo.md](mojo.md), [sandbox.md](sandbox.md)?)*