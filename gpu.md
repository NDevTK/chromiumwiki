# Component: GPU Process & Graphics Stack

## 1. Component Focus
*   **Functionality:** Manages interaction with the system's GPU hardware for accelerated rendering (Compositing via `viz`, WebGL, WebGPU, Canvas2D, video decoding/encoding). Runs in a separate, sandboxed process (`GpuProcessHost`, `GpuProcessHostUIShim`) for security and stability. Communicates with the Browser process and Renderer processes via IPC (primarily Mojo Command Buffers and specific interfaces).
*   **Key Logic:** GPU process launching and sandboxing (`GpuProcessHost`, `GpuSandboxedProcessLauncherDelegate`), establishing communication channels, managing GPU contexts (OpenGL ES/WebGL via ANGLE, Vulkan, Metal, Dawn for WebGPU), processing rendering commands (`CommandBufferStub`, `GLES2Implementation`), handling graphics memory (`GpuMemoryBuffer`), interacting with graphics drivers, managing GPU crashes (`OnProcessCrashed`), hardware video acceleration.
*   **Core Files:**
    *   `content/browser/gpu/gpu_process_host.*`: Manages the GPU process lifetime, launch, and communication channels from the browser side. See [gpu_process.md](gpu_process.md) for process details.
    *   `content/common/gpu/`: Shared GPU-related code and Mojo interfaces.
    *   `gpu/`: Core GPU abstraction layer.
        *   `gpu/command_buffer/client/`: Client-side command buffer logic (runs in Renderer).
        *   `gpu/command_buffer/service/`: Service-side command buffer logic (runs in GPU process, e.g., `command_buffer_stub.cc`, `gles2_cmd_decoder.cc`).
        *   `gpu/ipc/`: GPU IPC specific infrastructure.
    *   `components/viz/`: Compositor implementation.
    *   `ui/gl/`: OpenGL bindings and utilities.
    *   `third_party/angle/`: ANGLE library (OpenGL ES -> Desktop GL/Vulkan/Metal translation).
    *   `third_party/dawn/`: WebGPU implementation.
    *   `media/gpu/`: Hardware video acceleration integration.

## 2. Potential Logic Flaws & VRP Relevance
Vulnerabilities in the GPU process or stack can lead to sandbox escapes (often via driver bugs), information leaks (timing attacks, uninitialized memory), or denial of service.

*   **GPU Process Sandbox Escape:** Exploiting vulnerabilities in the GPU process itself or, more commonly, in the underlying graphics drivers it interacts with, to escape the sandbox and gain privileges.
    *   **Mechanism:** Sending crafted command buffer streams or inputs to WebGL/WebGPU APIs that trigger memory corruption (buffer overflows, UAF, type confusion) within the GPU process or driver code. Requires deep knowledge of specific APIs and driver behavior. Fuzzing is a primary method for finding these.
*   **Information Leaks (Side Channels):** GPU operations (rendering, texture uploads, shader compilation) taking variable time depending on input data, allowing cross-origin information leaks via timing attacks.
    *   **VRP Pattern (Canvas/WebGL Timing Leaks):** Timing differences in `CanvasRenderingContext2D.drawImage`, WebGL texture operations (`texImage2D`), CSS filters, or canvas composite operations depending on pixel data (especially alpha values or subnormal floats), enabling reconstruction of cross-origin images or inferring visited link status. (VRP2.txt#6693, #7318, #7346 - Canvas+SW, #16520 - WebGL texture errors leaking redirect URLs, #16755 - SVG filter timing, #16585 - Canvas composite timing, #16788 - WebGL capturing other tabs).
    *   **VRP Pattern (Uninitialized Memory):** Potential for uninitialized graphics memory (textures, buffers) being exposed to web content, leaking data from previous operations or other processes (less common in recent VRPs due to better memory management, but still a risk).
*   **Denial-of-Service (DoS):** Malicious web content triggering excessive GPU resource usage, command buffer errors, driver hangs, or GPU process crashes.
    *   **VRP Pattern (GPU Process Crash):** Triggering crashes via malformed inputs or demanding excessive resources.
*   **Incorrect Validation / State Handling:** Flaws in validating command buffer data, managing GPU state (contexts, resources), or handling errors/resets.
*   **API Implementation Bugs (WebGL/WebGPU):** Vulnerabilities within the implementation of specific WebGL or WebGPU features (shader compilation, texture handling, buffer management).
    *   **VRP Pattern (WebGL Context Attributes):** Leak on reload related to `getContextAttributes` (VRP2.txt#13376).

## 3. Further Analysis and Potential Issues
*   **Sandbox Strength:** How effective is the GPU process sandbox on different platforms? What driver interactions are permitted? Is Mojo interface exposure from the GPU process minimized?
*   **Command Buffer Validation (`GLES2CmdDecoder`):** This is a critical attack surface. How thoroughly are incoming commands and their arguments validated in the GPU process before being passed to the driver? Focus on buffer sizes, offsets, resource IDs, state consistency.
*   **Driver Interaction:** Vulnerabilities often lie within the closed-source drivers. How does Chromium mitigate driver risks? Are risky features blocklisted?
*   **Memory Management:** How are GPU memory buffers (`GpuMemoryBuffer`) allocated, shared between processes (via IPC), and tracked? Are there risks of UAF or information leaks via uninitialized memory?
*   **Timing Side Channels:** Identify GPU operations whose timing might vary based on input data (pixel values, shader complexity). Are constant-time principles applied where necessary? (VRP2.txt#6693, etc.).
*   **WebGPU Security:** As a newer, complex API, WebGPU (`Dawn`) requires thorough security review, especially regarding shader compilation (WGSL), resource management, and multi-process communication.
*   **Crash Handling (`GpuProcessHost::OnProcessCrashed`):** Ensure crashes are handled gracefully without leading to browser instability or further security issues. How is state recovered?

## 4. Code Analysis
*   `GpuProcessHost`: Manages GPU process lifecycle, launching (`LaunchGpuProcess`), sandboxing, and crash handling (`OnProcessCrashed`). Handles Mojo connection establishment. Checks 3D API blocklist (`BlockDomainsFrom3DAPIs`).
*   `GpuCommandBufferStub`: Service-side endpoint for command buffers. Routes commands to decoders.
*   `GLES2CmdDecoder`: Implements the GLES2 command buffer decoding logic. **Primary target for command injection/validation bugs.**
*   `Viz` components (`//components/viz/`): Handle compositing, potentially interacting directly with GPU resources.
*   ANGLE (`//third_party/angle/`): Translates GLES calls. Complex codebase, potential source of bugs interacting with native drivers.
*   Dawn (`//third_party/dawn/`): WebGPU implementation. Needs thorough review.
*   `gpu/ipc/common/gpu_memory_buffer.*`: Handling shared graphics memory.
*   WebGL/Canvas rendering paths in Blink (`//third_party/blink/renderer/modules/webgl/`, `//third_party/blink/renderer/platform/graphics/canvas/`). Check timing-dependent operations (VRP2.txt#6693).

## 5. Areas Requiring Further Investigation
*   **Command Buffer Fuzzing:** Aggressively fuzz the command buffer interface targeting the GPU process decoders (`GLES2CmdDecoder`, potentially WebGPU decoders).
*   **Driver Interaction Points:** Identify and analyze code paths where Chromium directly interacts with graphics drivers (e.g., context creation, resource allocation, command submission).
*   **Timing Attack Analysis:** Identify performance-critical graphics operations (blending, filtering, texture sampling, complex shader execution) and analyze their potential for data-dependent timing variations exploitable cross-origin (VRP2.txt#6693, #16755, etc.).
*   **WebGPU Implementation Audit:** Security review of the Dawn library and its integration, focusing on shader validation (WGSL), memory safety, and API validation.
*   **Sandbox Policy Review:** Regularly review and update the GPU process sandbox policies for each platform to minimize attack surface.
*   **Uninitialized Memory:** Use memory safety tools (ASan, MSan) to hunt for uninitialized memory issues in the graphics stack, especially related to shared memory buffers.

## 6. Related VRP Reports
*   VRP2.txt#6693, #7318, #7346, #16520, #16755, #16585, #16788 (Timing/Pixel Leaks via Canvas/WebGL/SVG Filters)
*   VRP2.txt#13376 (WebGL `getContextAttributes` leak on reload)
*   *(Note: Direct GPU process sandbox escapes often involve driver CVEs rather than specific Chromium VRPs, but Chromium aims to mitigate these via sandboxing and command buffer validation).*

## 7. Cross-References
*   [gpu_process.md](gpu_process.md)
*   [site_isolation.md](site_isolation.md) (GPU process is part of the isolation model)
*   [ipc.md](ipc.md) / [mojo.md](mojo.md) (Communication mechanism)
*   [webgl.md](webgl.md) (Specific API - if exists)
*   [webgpu.md](webgpu.md)
