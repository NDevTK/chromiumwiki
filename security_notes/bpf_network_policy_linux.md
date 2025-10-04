# Security Analysis of the Linux Network Sandbox Policy

## Overview

The file `sandbox/policy/linux/bpf_network_policy_linux.cc` defines the seccomp-bpf policy for the **Network Service process** on Linux. The network process is responsible for handling all of Chromium's network traffic, making it a high-value target for attackers. The goal of this sandbox policy is to restrict the kernel interface of the network process to only what is absolutely necessary for its operation, thereby limiting the damage that could be done if the process were compromised.

## A Permissive but Necessary Policy

Compared to the renderer sandbox, the network process sandbox is significantly more permissive. This is a necessary trade-off, as the network process must be able to perform a wide range of network-related system calls, such as `socket`, `bind`, `connect`, and `listen`. However, the policy still follows the principle of least privilege by restricting the parameters of these system calls wherever possible.

## Key Security-Relevant Logic in `EvaluateSyscall`

The core of the policy is the `EvaluateSyscall` method, which is a `switch` statement that evaluates each system call made by the network process.

### 1. Networking Syscalls

The policy allows a wide range of networking-related system calls, but with restrictions:

-   **`socket`**: The `RestrictSocketForNetworkService` function is used to restrict the `socket` system call. It only allows the creation of `AF_UNIX`, `AF_NETLINK`, `AF_INET`, and `AF_INET6` sockets. For each of these domains, it further restricts the allowed `type` and `protocol`. This is a critical security measure that prevents the network process from creating arbitrary types of sockets.

-   **`setsockopt` and `getsockopt`**: These system calls are also heavily restricted. The `RestrictSetSockoptForNetworkService` and `RestrictGetSockoptForNetworkService` functions use a `switch` statement to whitelist the allowed socket options. This prevents a compromised process from setting dangerous or unexpected socket options.

-   **Other Networking Syscalls**: Other networking-related system calls, such as `connect`, `bind`, `listen`, and `sendmmsg`, are currently allowed without restrictions. This is an area that could potentially be tightened in the future.

### 2. File System Access

The network process needs to access certain files, such as those related to network configuration and certificates. However, it should not have general access to the file system.

-   **Syscall Brokering**: The policy uses the **syscall broker** to handle file system access. When the network process needs to open a file, the `openat` system call is trapped and forwarded to the privileged browser process. The browser process then validates the request against a strict allowlist of paths before opening the file on behalf of the network process. This is a key security mechanism that allows the network process to access the files it needs without granting it broad file system access.

-   **`inotify`**: The `inotify` system calls are allowed, but `inotify_add_watch` is brokered. This allows the network process to monitor for changes to network configuration files without being able to add arbitrary watches.

### 3. Default-Deny Posture

As with other sandbox policies, the network process policy has a "default-deny" posture. Any system call not explicitly allowed by the policy will be blocked, and the process will be terminated. This is a fundamental security principle that helps to ensure that new or unexpected system calls are not accidentally allowed.

## Conclusion

The Linux network sandbox policy is a complex and carefully crafted set of rules that aims to balance the need for network access with the need for security. It uses a combination of syscall whitelisting, parameter filtering, and syscall brokering to restrict the network process to the minimum set of privileges it needs to function. While the policy is necessarily more permissive than the renderer sandbox, it still provides a strong security boundary that helps to protect the user's system from a compromised network process. The use of the syscall broker for file access is a particularly strong security feature.