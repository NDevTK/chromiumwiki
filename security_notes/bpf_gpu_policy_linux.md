# Linux GPU Sandbox Policy (`sandbox/policy/linux/bpf_gpu_policy_linux.cc`)

## 1. Summary

This file defines the `seccomp-bpf` system call (syscall) filter policy for the sandboxed **GPU process** on Linux. The GPU process is responsible for all communication with the system's graphics drivers, which are notoriously complex, closed-source, and a common source of security vulnerabilities.

The primary goal of this sandbox policy is **containment**. It assumes that a vulnerability in the graphics driver or the GPU command processing code might be triggered by malicious web content. The policy aims to severely limit the kernel attack surface available to a compromised GPU process, preventing it from doing anything other than its intended graphics-related tasks, thereby stopping an attacker from escalating a GPU process compromise into a full system compromise.

## 2. Core Concepts

*   **Layered Policy:** The `GpuProcessPolicy` inherits from `BPFBasePolicy`. This means it starts with a foundational set of "safe" syscalls (for memory management, basic I/O on existing file descriptors, etc.) and then adds the specific, more powerful syscalls that are required for GPU operations.

*   **A Necessary Trade-off:** Compared to the renderer sandbox, the GPU sandbox policy is significantly more permissive. This is a deliberate security trade-off. Graphics drivers require access to a wide range of powerful and often poorly documented kernel interfaces (especially `ioctl`) to function. The policy must allow these operations while still blocking obviously dangerous syscalls like `fork`, `execve`, or `socket`.

*   **Syscall Brokering:** This is a key security pattern used by the GPU sandbox. For syscalls that are too dangerous to allow directly but are sometimes needed (like opening a file), the policy uses a `Trap` mechanism.
    *   When the GPU process makes a syscall like `openat`, the seccomp filter traps it.
    *   The kernel passes control to a signal handler inside the sandboxed process.
    *   This handler forwards the syscall and its arguments over an IPC pipe to the privileged browser process.
    *   The browser process (`BrokerHost`) receives the request, validates the arguments against a strict allowlist (e.g., "is this a known driver file or shader cache directory?"), and, if it's safe, performs the syscall on the GPU process's behalf, returning the result (e.g., a file descriptor).
    *   This allows the GPU process to open specific, required files without having general filesystem access.

## 3. Security-Critical Logic & Vulnerabilities

*   **`ioctl` Permissiveness:**
    *   **Risk:** The single most significant security risk is the broad allowance of the `ioctl` syscall. `ioctl` is a massive multiplexer that provides a gateway to thousands of different kernel driver commands. A vulnerability in a driver's `ioctl` handler is the most likely vector for a GPU process compromise.
    *   **Mitigation:** The sandbox does not attempt to filter the arguments to `ioctl` because the set of valid commands is enormous and driver-specific. Instead, the security model accepts this risk and relies on the *rest* of the sandbox to contain the damage. A compromised process that exploits an `ioctl` bug will still be unable to execute new processes, access the network, or read arbitrary files.

*   **Dynamic Library Loading (`dlopen`):**
    *   **Risk:** The policy explicitly allows the family of syscalls needed to implement `dlopen`. This is necessary because many graphics drivers are designed to dynamically load helper libraries at runtime. This presents a risk: if an attacker could write a malicious `.so` file to a location on disk that the GPU process can access, they could load it and achieve code execution.
    *   **Mitigation:** The security relies entirely on the filesystem permissions being correctly configured *before* the sandbox is engaged. The browser process ensures that the GPU process only has read access to a very specific allowlist of directories containing legitimate driver files.

*   **Syscall Brokering Logic:**
    *   **Risk:** The security of the syscall brokering mechanism depends on the integrity of both the trapping mechanism in the sandbox and the validation logic in the browser process. A bug that allowed a syscall to bypass the broker and be executed locally, or a flaw in the browser's path validation logic, could allow the GPU process to open an unauthorized file.
    *   **Mitigation:** The `ShouldBrokerHandleSyscall` check and the `HandleViaBroker` implementation are critical. The browser-side `BrokerHost` must maintain a strict allowlist of paths.

## 4. Key Functions

*   `GpuProcessPolicy::EvaluateSyscall(int sysno)`: The main entry point for the policy. It's a `switch` statement that defines the allowlist of syscalls specific to the GPU process.
*   `sandbox_linux->HandleViaBroker(sysno)`: The function call that executes the "trap and forward" logic for syscalls that must be handled by the privileged browser process.
*   `Restrict...` functions (e.g., `RestrictPrlimit64`): Helper functions that return a BPF program that allows a syscall but restricts its arguments to prevent privilege escalation.

## 5. Related Files

*   `sandbox/policy/linux/bpf_base_policy_linux.cc`: The base policy from which the GPU policy inherits.
*   `sandbox/linux/syscall_broker/broker_process.h` and `broker_host.h`: Define the client and server sides of the syscall brokering mechanism.
*   `content/gpu/gpu_main.cc`: The main entry point for the GPU process, where the sandbox policy is initialized and engaged.
*   `content/browser/gpu/gpu_process_host.cc`: The browser-side code that launches the GPU process and is responsible for setting up its sandbox and brokering its syscalls.