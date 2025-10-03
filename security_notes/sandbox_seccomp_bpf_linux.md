# Security Notes: `sandbox/policy/linux/sandbox_seccomp_bpf_linux.cc`

## File Overview

This file is the core of Chromium's sandboxing mechanism on Linux. It acts as the central policy dispatcher for the `seccomp-bpf` sandbox, which is responsible for filtering and restricting the system calls (syscalls) a process is allowed to make. By limiting the kernel attack surface accessible to a process, this is arguably the most important layer of the Linux sandbox. This file maps a high-level process type to a specific, granular syscall allowlist.

## Key Security-Relevant Components and Patterns

### 1. The Policy Dispatcher: `PolicyForSandboxType`

This function is the heart of the file. It implements a large `switch` statement that takes a `sandbox::mojom::Sandbox` type and returns the corresponding `BPFBasePolicy` object.

-   **Mechanism**: Based on the process type (e.g., `kRenderer`, `kGpu`, `kNetwork`), it instantiates a specific policy class (e.g., `RendererProcessPolicy`, `GpuProcessPolicy`).
-   **Security Implication**: This is the primary security policy decision point for the entire Linux sandbox. The security of each process type is dictated by the policy selected here. Any mistake in this logic, such as assigning a weaker policy to a process that handles untrusted data, would be a critical vulnerability.
-   **Layered Policies**: Each of these specific policies inherits from `BaselinePolicy`, which provides a common set of restrictions for all sandboxed processes. The specific policies then add the additional syscalls needed for their particular task. This layered approach is a strong security design, ensuring a "deny by default" posture.

### 2. Post-Activation Sanity Checks: `RunSandboxSanityChecks`

This function is a powerful and critical security feature. After the seccomp-bpf sandbox has been activated, this code runs for certain process types to *verify* that the sandbox is actually working as intended.

-   **Mechanism**: The function executes syscalls that should be forbidden by the policy, such as `open("/etc/passwd", O_RDONLY)` or `socket(AF_NETLINK, ...)`.
-   **Verification**: It then uses `CHECK` assertions to confirm two things:
    1.  The syscall failed (returned -1).
    2.  The `errno` is set to the *exact* value expected from a seccomp-bpf violation (usually `EPERM` or `EACCES`).
-   **Security Implication**: This is not just a test; it's a runtime assertion that provides extremely high confidence that the sandbox is active and enforcing the rules. It defends against kernel bugs, policy misconfigurations, or other unexpected states that might cause the sandbox to fail silently. This is a hallmark of a mature and robust security system.

### 3. Sandbox Kill Switches: `IsSeccompBPFDesired`

This function determines whether the seccomp-bpf sandbox should be enabled at all.

-   **Mechanism**: It checks for the presence of two command-line flags: `--no-sandbox` and `--disable-seccomp-filter-sandbox`.
-   **Security Implication**: This is the global "off switch" for this security layer. While necessary for debugging and testing, it's a critical point of interest for a security researcher. Understanding how these flags can be set or manipulated is key to assessing the real-world security posture of the browser.

### 4. Hardware-Specific Policies (GPU Process)

The logic in `GetGpuProcessSandbox` reveals the complexity of sandboxing the GPU process.

-   **Mechanism**: Instead of a single GPU policy, it selects a different one based on the underlying hardware (ARM, AMD, Intel, NVIDIA) and platform (ChromeOS).
-   **Security Implication**: The GPU process has a significantly larger and more complex attack surface than most other processes because it needs to interact directly with kernel-mode drivers. The need for vendor-specific syscall policies highlights this complexity. A vulnerability in a graphics driver is a common way to escape a sandbox, making the GPU process policies a high-value target for security analysis.

### 5. The One-Way Nature of Seccomp

A comment in `StartSandboxWithExternalPolicy` reinforces a fundamental security property of seccomp:

```cpp
// Starting the sandbox is a one-way operation. The kernel doesn't allow
// us to unload a sandbox policy after it has been started.
```

-   **Security Implication**: This is the "no-return" feature of seccomp. Once a process enters the sandbox, it cannot be disabled or weakened from within the process. This prevents an attacker who gains code execution in a sandboxed process from simply turning off the sandbox to gain full access to the kernel.

## Summary of Security Posture

`sandbox_seccomp_bpf_linux.cc` demonstrates a very strong and well-designed security architecture.

-   **Defense-in-Depth**: It uses a layered policy model (baseline + specific) and combines this with runtime sanity checks to verify its own operation.
-   **Centralized Policy Decisions**: The `PolicyForSandboxType` function provides a single, auditable location where security policies are assigned to process types.
-   **Graceful Degradation**: The code checks for kernel support (`SupportsSandbox`) and will not engage the sandbox if the host system is not capable, preventing crashes.
-   **High-Value Targets for Audit**: For a researcher, the most interesting areas are the specific policy implementations themselves (e.g., `bpf_renderer_policy_linux.cc`) and the complex, hardware-dependent GPU policies.