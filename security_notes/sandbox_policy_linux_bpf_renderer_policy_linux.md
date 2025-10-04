# Security Analysis of the Linux Renderer Sandbox Policy

## Overview

The file `sandbox/policy/linux/bpf_renderer_policy_linux.cc` defines the seccomp-bpf (secure computing mode with Berkeley Packet Filter) policy for the renderer process on Linux. This policy is a critical component of the sandbox, as it specifies the exact set of system calls that a renderer process is allowed to make. By restricting the kernel attack surface available to the renderer, this policy significantly mitigates the damage that can be done by a compromised renderer process.

## The BPFBasePolicy Foundation

The `RendererProcessPolicy` inherits from `BPFBasePolicy`. This base policy provides a common set of system calls that are considered safe for most sandboxed processes. These typically include fundamental operations like memory management (`mmap`, `brk`), inter-process communication with the browser (`read`, `write` on specific file descriptors), and basic process control (`exit`, `futex`).

The `RendererProcessPolicy` then builds upon this baseline, adding or restricting syscalls as needed for the specific requirements of rendering web content.

## Key Security-Relevant Logic in `EvaluateSyscall`

The core of the policy is the `EvaluateSyscall` method. This method is a giant `switch` statement that is evaluated for every system call made by the sandboxed process.

### 1. Explicitly Allowed Syscalls

The policy explicitly allows a small number of additional system calls that are necessary for the renderer's operation:

-   **`clock_getres`**: Allowed for V8's time measurement needs.
-   **`fsync`, `fdatasync`, `ftruncate`**: Basic file synchronization and truncation operations.
-   **`getrlimit`, `setrlimit`, `prlimit64`**: These are crucial for WebAssembly. The renderer needs to be able to adjust its own address space limits to manage large WebAssembly modules. The policy allows this but relies on the kernel's protection that a process cannot raise its `rlim_max` once it has been lowered, which prevents a compromised renderer from giving itself unlimited memory.
-   **`mremap`**: Allows for remapping memory regions, which is used by V8.
-   **`pwrite64`**: Allows for writing to a file at a specific offset.
-   **`sched_get_priority_max`, `sched_get_priority_min`**: Used for thread scheduling.
-   **`uname`, `sysinfo`**: Provide basic system information.
-   **`getcpu`**: Allowed on ARM architectures for performance reasons (`//third_party/cpuinfo`).

### 2. Parameter-Filtered Syscalls

For some powerful system calls, the policy allows the call but restricts its parameters. This is a key security technique that allows for fine-grained control.

-   **`ioctl`**: This is a very powerful system call that can be used to control devices. The policy uses a `Switch` statement to only allow a small, whitelisted set of `ioctl` requests, such as `TCGETS` (for terminal control) and `FIONREAD` (to get the number of bytes available to read). Any other `ioctl` request results in the process being terminated with a `SIGSYS` signal (`CrashSIGSYSIoctl`).
-   **`prctl`**: Another powerful system call for process control. On ARM architectures, the policy only allows `PR_SVE_GET_VL` and `PR_SME_GET_VL`, which are related to the Scalable Vector Extension. All other `prctl` options are denied by falling through to the base policy.

### 3. Default-Deny Posture

The most important security aspect of the policy is its "default-deny" posture. The final `default` case in the `switch` statement is:

```cpp
default:
  // Default on the content baseline policy.
  return BPFBasePolicy::EvaluateSyscall(sysno);
```

This means that any system call not explicitly handled by the `RendererProcessPolicy` is passed down to the `BPFBasePolicy`. The base policy, in turn, has its own whitelist. Any syscall not on *that* list will be denied, resulting in the termination of the process. This ensures that new or unexpected system calls are blocked by default, which is a fundamental principle of robust sandboxing.

## Conclusion

The Linux renderer sandbox policy is a strong example of the principle of least privilege in action. It starts with a restrictive baseline policy and then carefully opens up the minimum set of system calls necessary for the renderer to function. The use of parameter filtering on powerful system calls like `ioctl` and `prctl` provides an additional layer of security. This fine-grained control over the kernel attack surface is a cornerstone of Chromium's security model on Linux.