# Chromium Sandbox Architecture

## Overview

Chromium's security model is built on the principle of sandboxing, which aims to run untrusted code in a restricted environment with limited privileges. This document provides a high-level overview of the general sandboxing architecture, which is common across different platforms.

## The Broker Process Model

The core of Chromium's sandboxing architecture is the **broker process model**. This model involves two processes:

1.  **The Broker Process**: This is a privileged process (typically the main browser process) that has access to system resources.
2.  **The Target Process**: This is a less-privileged, sandboxed process (e.g., a renderer process) that runs untrusted code.

The target process is not allowed to directly access most system resources, such as the file system or the network. Instead, when it needs to perform a privileged operation, it sends an Inter-Process Communication (IPC) request to the broker process. The broker process then validates the request and, if it's deemed safe, performs the operation on behalf of the target process.

This architecture ensures that even if an attacker compromises the target process, they are still contained within the sandbox and cannot directly harm the user's system.

## Sandbox Policies

Chromium does not use a one-size-fits-all sandbox. Instead, it applies different **sandbox policies** to different types of processes, based on the principle of least privilege. The `sandbox/policy/sandbox_type.cc` file defines the various sandbox types, such as:

*   `kRenderer`: For renderer processes that handle web content. This is the most restrictive sandbox.
*   `kGpu`: For the GPU process, which has access to the graphics driver.
*   `kNetwork`: For the network service, which handles all network requests.
*   `kAudio`: For the audio service.
*   `kCdm`: For Content Decryption Modules, which handle protected media.

Each of these sandbox types has a corresponding policy that defines exactly what system resources the process is allowed to access. For example, the renderer process is blocked from making direct network requests, while the network process is allowed to.

## Platform-Specific Implementations

The actual enforcement of the sandbox policies is highly platform-specific:

*   **Linux**: The sandbox is primarily implemented using **seccomp-bpf**, which allows the browser to define a whitelist of allowed system calls for each process type. It also uses **namespaces** to isolate the process from the rest of the system. The `sandbox/policy/linux/bpf_renderer_policy_linux.cc` file is a good example of a seccomp-bpf policy.

*   **Windows**: The sandbox is implemented using a combination of **restricted tokens**, **Job Objects**, and **interceptors**. A restricted token is a special type of access token that has reduced privileges. Job Objects are used to apply restrictions to a group of processes. Interceptors are used to hook system calls and redirect them to the broker process. The `sandbox/win/src/broker_services.h` file defines the interface for the broker process on Windows.

*   **macOS**: The sandbox is implemented using the built-in **Seatbelt** technology, which allows the browser to define a set of rules that restrict the process's access to files and other system resources.

## IPC and Mojo

The communication between the broker and target processes is handled by Chromium's IPC and Mojo systems. Mojo is a modern IPC framework that is used for most new inter-process communication in Chromium. It provides a secure and reliable way for processes to communicate, even when they are running in different sandboxes.

## Conclusion

Chromium's sandboxing architecture is a complex, multi-layered system that is designed to provide robust protection against a wide range of attacks. Its key features include the broker process model, policy-based restrictions, and platform-specific enforcement mechanisms. A deep understanding of this architecture is essential for any developer working on Chromium security.