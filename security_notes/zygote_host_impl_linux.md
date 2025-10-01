# Linux Zygote Host (`content/browser/zygote_host/zygote_host_impl_linux.h`)

## 1. Summary

The `ZygoteHostImpl` is a singleton in the browser process that is responsible for launching and managing the "Zygote" process on Linux and ChromeOS. The Zygote is a fundamental part of Chromium's security and performance strategy on these platforms. It is a special process that is started early, pre-initializes libraries, and then enters a state where it can be commanded to `fork()` itself to create new, sandboxed renderer processes.

This class is a highly critical security component. It orchestrates the creation of the sandbox boundary for all renderers. A vulnerability in the `ZygoteHostImpl` or a misconfiguration during Zygote launch could lead to weak or non-existent sandboxing for all web content, effectively neutralizing a primary layer of Chromium's security architecture.

## 2. Core Concepts

*   **Forking Server Model:** The Zygote acts as a "forking server." Instead of starting a new renderer process from scratch for every tab (which is slow), the browser tells the already-running Zygote to `fork()`. The new child process inherits the pre-initialized state of the Zygote, making process creation much faster.

*   **Pre-Sandbox Initialization:** The Zygote starts *before* the seccomp-bpf sandbox is engaged. This allows it to perform privileged operations that will be forbidden later, such as opening certain libraries or establishing communication channels. Once it's ready, it drops privileges and waits for commands.

*   **Two-Phase Sandbox:** Chromium uses a two-layered sandbox on Linux:
    1.  **The Zygote Sandbox:** The Zygote itself runs in a light sandbox.
    2.  **The Renderer Sandbox:** After the Zygote forks, the new child process enables a much stricter `seccomp-bpf` sandbox policy (defined in `bpf_renderer_policy_linux.cc`) before it begins processing any web content.

*   **Control Channel:** The `ZygoteHostImpl` communicates with the Zygote process over a dedicated, privileged IPC channel (`control_fd`). All commands, such as "fork a new renderer," are sent over this channel.

## 3. Security-Critical Logic & Vulnerabilities

*   **Zygote Launch Configuration:**
    *   **Risk:** The `LaunchZygote` method is the most security-critical part of this class. It constructs the `base::CommandLine` that will be used to execute the Zygote process. If an attacker could influence these command-line arguments, they could disable the sandbox (`--no-sandbox`) or specify a weakened seccomp policy.
    *   **Mitigation:** The command line is constructed internally based on feature flags and build configuration. It is not derived from untrusted renderer input. The `Init` method reads the browser's own command line to determine which sandbox type (`namespace`, `suid`) to use.

*   **Control Channel (FD) Integrity:**
    *   **Risk:** The file descriptor for the control channel is a highly sensitive capability. If a compromised renderer could gain access to this FD, it could send forged messages to the Zygote, potentially telling it to fork a new process with a disabled sandbox or other incorrect parameters.
    *   **Mitigation:** Standard OS-level process separation prevents a child process from accessing its parent's file descriptors unless they are explicitly passed down. The security of the system relies on this FD never being leaked to a less-privileged process.

*   **SUID Sandbox Interaction:**
    *   **Risk:** The `use_suid_sandbox_` flag indicates that the Zygote is interacting with a setuid root helper binary. This is a classic privilege boundary. Any communication with this helper (e.g., via `AdjustRendererOOMScore`) must be extremely careful to prevent command injection or other exploits that could lead to privilege escalation.
    *   **Mitigation:** The communication protocol with the setuid helper is designed to be very simple and restrictive, typically only involving passing integers for specific, well-defined operations.

*   **PID Management:**
    *   **Risk:** The `ZygoteHostImpl` tracks the process IDs of its Zygotes (`zygote_pids_`). If this mapping were to be corrupted, the browser might fail to correctly identify the Zygote, potentially leading to a failure to terminate it correctly or sending privileged commands to the wrong process.
    *   **Mitigation:** The PIDs are added by the trusted `LaunchZygote` method and are protected by a lock (`zygote_pids_lock_`).

## 4. Key Functions

*   `Init(const base::CommandLine& cmd_line)`: Initializes the host, reading security-related command-line switches to determine the sandbox configuration.
*   `LaunchZygote(...)`: The core function that constructs the command line, launches the Zygote helper process, and establishes the control FD.
*   `AdjustRendererOOMScore(...)`: An example of a privileged operation that is brokered by the Zygote host, potentially involving communication with a setuid helper.
*   `IsZygotePid(pid_t pid)`: A security-relevant check to distinguish the special Zygote process from a regular child process.

## 5. Related Files

*   `content/zygote/zygote_main_linux.cc`: The `main()` function for the Zygote process itself. This is the code that runs on the other side of the control channel, receiving and acting on commands from the `ZygoteHostImpl`.
*   `sandbox/linux/suid/client/setuid_sandbox_client.h`: The client library for communicating with the setuid sandbox helper binary, used for operations that require root privileges.
*   `sandbox/policy/linux/sandbox_linux.cc`: The class that uses the Zygote to launch new sandboxed processes.
*   `content/public/common/content_switches.h`: Defines the command-line flags (`--no-sandbox`, etc.) that are parsed by `ZygoteHostImpl` to configure the sandbox.