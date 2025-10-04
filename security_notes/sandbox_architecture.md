# An Architectural Overview of the Chromium Sandbox

## 1. High-Level Concept: The Broker/Target Model

Chromium's security model is built on the principle of sandboxing, which aims to contain the damage from a compromised process. The core architectural pattern is the **broker/target model**:

*   **The Broker**: The main browser process. It runs with the user's full privileges and acts as a trusted gatekeeper to the operating system. It is responsible for all sensitive operations, such as accessing the filesystem, network, and user profile data.
*   **The Target**: A sandboxed process (e.g., renderer, GPU, utility process). These processes are "locked down" in a restrictive sandbox and run with minimal privileges. They are considered untrustworthy. When a target process needs to perform a privileged action, it must make a request to the broker via Inter-Process Communication (IPC).

The sandbox's primary goal is to ensure that even if an attacker achieves remote code execution in a target process (e.g., by exploiting a bug in the Blink rendering engine), they cannot harm the underlying system. The sandbox acts as a second line of defense.

## 2. The Cross-Platform Abstraction Layer

The entry point for sandboxing policy is `sandbox/policy/`. The `sandbox/policy/sandbox.h` header provides the main cross-platform interface.

*   **`Sandbox::Initialize()`**: This static method is called by the broker process to launch a new sandboxed target process. It takes a `sandbox::mojom::Sandbox` enum type, which specifies the type of sandbox to apply (e.g., `kRenderer`, `kGpu`, `kNetwork`).
*   **`Sandbox::IsProcessSandboxed()`**: This static method is intended to be the canonical check for whether the current process is running inside a sandbox.

### Security Warning: The `--no-sandbox` Deception

The `IsProcessSandboxed()` function contains a critical "gotcha". For non-browser processes, if the `--no-sandbox` command-line flag is present, **the function lies and returns `true`**. This is done for the convenience of `DCHECK` assertions in test environments. However, it makes this function **inherently unsafe for any runtime security decisions**. Any code that uses this check to decide whether a privileged operation is safe will be completely bypassed when the `--no-sandbox` flag is used.

## 3. The Windows Sandbox

The Windows sandbox is a mature and robust implementation that leverages several OS-level security features.

*   **Broker/Target Discovery**: The determination of whether a process is a broker or a target is made via a named shared memory section. The broker creates it; targets open a handle to it. The presence of this handle (`g_shared_section` in `sandbox/win/src/sandbox.cc`) is the definitive, reliable check for whether a process is a target.
*   **Restricted Tokens & Integrity Levels**: The broker launches target processes with a highly restricted access token and at a very low "integrity level" (e.g., `INTEGRITY_LEVEL_LOW`). This is the core of the sandbox; the Windows kernel enforces rules that prevent low-integrity processes from modifying or accessing high-integrity resources.
*   **Job Objects**: All target processes are placed in a Windows Job Object. This ensures that if the main broker process is terminated, the OS will automatically terminate all of its child (target) processes, preventing them from becoming orphaned.
*   **Win32k Lockdown**: For the most sensitive processes like the renderer, the sandbox applies a "win32k lockdown" policy. This severely restricts or completely blocks access to the Windows USER and GDI kernel components, which have historically been a major source of security vulnerabilities. This dramatically reduces the kernel attack surface available from the sandbox.
*   **AppContainer**: On modern versions of Windows, Chromium uses AppContainer, an even stronger isolation mechanism. It creates a virtualized environment for the process with a private registry, filesystem view, and a fine-grained capability-based model for accessing external resources.

## 4. The Linux Sandbox

The Linux sandbox uses a defense-in-depth strategy with two distinct layers of protection.

*   **Layer 1: The SUID Sandbox & Namespaces**:
    *   The primary sandbox is a small SetUID executable that uses Linux namespaces to create an isolated environment for the target process *before* it starts.
    *   It creates new **PID namespaces** (so the target can only see itself and its descendants), **network namespaces** (to prevent direct network access), and sometimes **user namespaces** (to run as a non-root user even inside the namespace).

*   **Layer 2: Seccomp-BPF Syscall Filtering**:
    *   After the process is isolated with namespaces, the `sandbox_seccomp_bpf_linux.cc` component applies a second layer of defense: a **seccomp-bpf filter**.
    *   This is a kernel-level filter that inspects every system call the process attempts to make. It operates on a strict allow-list principle: any syscall that is not explicitly permitted by the policy is denied, and the process is killed.
    *   **The Dispatcher**: The `PolicyForSandboxType` function in this file is the central dispatcher. It selects a highly specific, tailored policy for each sandbox type (`kRenderer`, `kGpu`, etc.). This adherence to the **principle of least privilege** is a key strength of the Linux sandbox. For example, the GPU policy is further specialized based on the hardware vendor to minimize the `ioctl` attack surface.
    *   **Sanity Checks**: After the seccomp-bpf policy is applied, the process runs a series of sanity checks (e.g., trying to call a forbidden syscall) to ensure the filter is active. If the check fails, the process crashes immediately, preventing it from running in an insecure state.

## 5. Summary

Chromium's sandboxing architecture is a mature, multi-layered, and platform-specialized defense against compromise.

*   **Strengths**:
    *   Strong reliance on OS-provided security primitives.
    *   Defense-in-depth approach, especially on Linux.
    *   Strict adherence to the principle of least privilege through specialized policies.
*   **Areas of Security Focus**:
    *   **Policy Correctness**: The security of the sandbox depends on the correctness and minimality of the individual policies (especially the seccomp-bpf syscall lists). An overly permissive policy is a sandbox escape.
    *   **Broker Logic**: As the trusted intermediary, any vulnerability in the broker process's IPC handling logic could be used by a sandboxed process to escape.
    *   **Initialization**: The sandbox must be initialized very early in the process lifecycle. Any code that runs before the sandbox is fully engaged is a potential source of vulnerabilities.
    *   **The `--no-sandbox` Flag**: This flag completely disables this critical security layer and should never be used in a production environment. The deceptive nature of `IsProcessSandboxed()` when this flag is used is a notable risk.