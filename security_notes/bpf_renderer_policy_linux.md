# Linux Renderer Sandbox Policy (`sandbox/policy/linux/bpf_renderer_policy_linux.cc`)

## 1. Summary

This file defines the `seccomp-bpf` sandbox policy for the most security-critical process in Chromium: the **renderer process**. On Linux, the sandbox works by installing a system call (syscall) filter that intercepts every syscall the process makes and decides whether to allow it, deny it with an error, or terminate the process for making a forbidden request.

This policy is the final line of defense within the renderer sandbox. If a renderer process is compromised by a vulnerability in V8 or Blink, this syscall filter is what prevents the attacker from using the kernel's interface to compromise the entire system (e.g., by opening arbitrary files, spawning new processes, or making network connections). Its correctness and restrictiveness are absolutely paramount to the security of the browser.

## 2. Core Concepts

*   **`seccomp-bpf`:** This is a Linux kernel feature that allows a process to install a BPF (Berkeley Packet Filter) program that is executed by the kernel for every syscall the process makes. The program's return value determines the fate of the syscall. Chromium uses this to create a highly restrictive allowlist of syscalls.

*   **Inheritance and Layering:** The renderer policy does not start from scratch. It inherits from `BPFBasePolicy`, which itself defines a baseline set of syscalls that are considered safe for most sandboxed process types (e.g., memory management syscalls like `mmap`, basic file descriptor operations on existing FDs like `read` and `write`). The `RendererProcessPolicy` then adds the *additional* syscalls that are specifically required for the renderer to function.

*   **Policy as a `switch` Statement:** The core of the policy is the `EvaluateSyscall` method, which is a giant `switch` statement on the syscall number. This design makes the policy explicit and auditable. If a syscall is not listed in the `switch` statement, it falls through to the `BPFBasePolicy`, which will likely deny it, causing the process to be killed.

## 3. Security-Critical Logic & Vulnerabilities

The entire file is security-critical. Adding a new syscall to the allowlist must be done with extreme care.

*   **The Allowlist Philosophy:** The fundamental security model is an **allowlist**. Any syscall not explicitly permitted is denied. A bug that accidentally adds a dangerous syscall to this list could create a hole in the sandbox. For example, allowing `openat` without extremely strict path restrictions would allow a compromised renderer to read arbitrary files on the filesystem.

*   **Argument-Based Restrictions:** Allowing a syscall is often not enough; its arguments must also be restricted.
    *   **Example (`prlimit64`):** The policy allows the `prlimit64` syscall, which could be dangerous if it allowed a process to raise its own resource limits. However, the implementation calls `RestrictPrlimit(GetPolicyPid())`, which adds a BPF filter to ensure that the syscall can only be used to *lower* limits or query them, not raise them.
    *   **Example (`ioctl`):** The `ioctl` syscall is a multiplexer for thousands of different device commands. Allowing it wholesale would be a massive security risk. The policy uses `RestrictIoctl` to create a nested `switch` statement that only allows a tiny, hand-picked subset of `ioctl` commands that are known to be safe and necessary (e.g., `TCGETS` for terminal information).

*   **WebAssembly and `mremap`:** The policy explicitly allows `mremap`.
    *   **Reason:** This is required for WebAssembly to efficiently grow its linear memory.
    *   **Security Risk:** While necessary, `mremap` is a powerful memory management tool. A bug in V8's WebAssembly implementation combined with a kernel vulnerability in `mremap` could potentially be a vector for exploitation. The security of this syscall relies on the V8 sandbox and other mitigations to ensure that the pointers and sizes passed to it are valid.

*   **Fall-through to Base Policy:** A syscall not handled in `RendererProcessPolicy` is passed to `BPFBasePolicy::EvaluateSyscall`. The security of the renderer also depends on the correctness of this base policy. A dangerous syscall accidentally allowed in the base policy would be inherited by all sandboxed processes, including the renderer.

## 4. Key Functions

*   `RendererProcessPolicy::EvaluateSyscall(int sysno)`: The main entry point for the policy. This is where the `switch` statement lives and where all decisions are made.
*   `RestrictIoctl()`: A critical helper function that demonstrates the principle of restricting syscall arguments, not just the syscall itself.
*   `RestrictPrlimit()`: Another example of restricting arguments to prevent a process from elevating its own privileges.

## 5. Related Files

*   `sandbox/policy/linux/bpf_base_policy_linux.cc`: The base policy from which the renderer policy inherits. Its allowlist is implicitly part of the renderer's allowlist.
*   `sandbox/policy/linux/sandbox_linux.cc`: The class responsible for taking a policy object (like `RendererProcessPolicy`) and actually installing it into the kernel using the `seccomp-bpf` API before the process is fully started.
*   `sandbox/linux/bpf_dsl/bpf_dsl.h`: The header for the BPF Domain-Specific Language, which provides the `Allow()`, `Error()`, `If()`, `Switch()`, and `Arg()` constructs used to build the policies in a readable way.