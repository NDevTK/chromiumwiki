# Security Analysis of mojo/public/cpp/bindings/sync_call_restrictions.h

## 1. Overview

`mojo/public/cpp/bindings/sync_call_restrictions.h` is a fundamental security component within the Mojo IPC framework. Its primary purpose is to provide a mechanism to enforce policies around synchronous Mojo calls. Synchronous IPC is a known source of security and stability issues, including deadlocks, performance bottlenecks, and complex re-entrancy vulnerabilities. This header is the central point of control for mitigating these risks.

In security-sensitive processes like the browser and GPU processes, synchronous IPC is disallowed by default to protect against attacks from less-privileged child processes. This component provides the mechanism for enforcing this default-deny policy and managing a tightly controlled set of exceptions.

## 2. Core Components

The key components defined in this file are:

*   **`SyncCallRestrictions` Class**: This class provides the core static methods for managing synchronous call policies.
    *   `DisallowSyncCall()`: Globally disables synchronous calls within the current process. This is called during the startup of the **browser process** (`content/browser/browser_main_loop.cc`) and the **GPU process** (`content/gpu/gpu_main.cc`), establishing a default-deny policy for sync IPC in these critical processes.
    *   `AssertSyncCallAllowed()`: Asserts that a synchronous call is permitted on the current sequence, causing a `DCHECK` failure if it is not. This is the primary enforcement point called by the Mojo bindings before processing a sync call.
    *   `DisableSyncCallInterrupts()`: A critical mitigation that changes the behavior of blocking sync calls. When interrupts are disabled, a thread waiting for a sync reply will *not* process any other incoming sync messages. This is crucial for preventing re-entrancy attacks.

*   **`ScopedAllowSyncCall` Class**: This is an RAII-style helper class that provides a mechanism to temporarily bypass the synchronous call restriction on a specific sequence. Its usage is tightly controlled via a `friend` class allowlist.

## 3. The `friend` Allowlist: A Critical Security Boundary

The `ScopedAllowSyncCall` constructor is private, and its use is restricted to a small, explicitly defined set of `friend` classes. This "allowlist" is the most critical security boundary in this component, as it enumerates the exact locations in the codebase that are permitted to violate the default no-sync-IPC policy. Any change to this list has significant security implications and requires careful review.

As of this writing, the allowlist includes:

*   `content::SynchronousCompositorHost`
*   `viz::GpuHostImpl`
*   `viz::HostFrameSinkManager`
*   `viz::HostGpuMemoryBufferManager`
*   `ui::Compositor`
*   `chromeos::ChromeOsCdmFactory`
*   `chromecast::CastCdmOriginProvider`
*   `content::AndroidOverlaySyncHelper`
*   `gpu::GpuChannelHost`
*   `gpu::CommandBufferProxyImpl`
*   `gpu::SharedImageInterfaceProxy`
*   `content::DCOMPTextureFactory` (Windows-only)
*   `web_app::WebAppShortcutCopierSyncCallHelper` (macOS-only)

These classes are primarily related to graphics, compositing, and platform-specific media handling, where synchronous communication with the GPU process has been deemed necessary for performance or correctness.

## 4. Security History: Re-entrancy, UAFs, and Mitigation

The history of this component is deeply intertwined with the discovery and mitigation of severe re-entrancy vulnerabilities.

**Issue 40061398: "Security: Design flaw in Synchronous Mojo message handling introduces unexpected reentrancy and allows for multiple UAFs"**

This critical vulnerability report from Google Project Zero detailed how Mojo's synchronous message handling could be abused to create use-after-free vulnerabilities. The core of the issue was:

1.  When a thread sends a synchronous message, it blocks waiting for a reply.
2.  By default, this waiting thread would process *other incoming synchronous messages* to prevent deadlocks.
3.  This behavior created an **unexpected re-entrancy point**. An attacker in a child process could send a carefully crafted message that would be processed by the waiting browser thread.
4.  This nested message handler could then execute code that would free an object that the original, outer function still had a raw pointer to on its stack.
5.  When the original sync call finally returned, the outer function would resume execution with a dangling pointer, leading to a UAF.

A key aggravating factor was that a compromised renderer could force any async method to be handled synchronously, dramatically increasing the attack surface.

**Mitigations:**

The fix for this class of vulnerabilities involved two key changes:

1.  **Disabling Sync Call Interrupts**: The default behavior in the browser process was changed to be non-interruptible (`DisableSyncCallInterrupts()`). This means that a thread blocking on a sync call will no longer process nested sync messages, completely closing the re-entrancy vector.
2.  **Validating the `[Sync]` flag**: The Mojo bindings were hardened to ensure that only methods explicitly marked as `[Sync]` in the `.mojom` file can be dispatched as synchronous calls. This prevents attackers from elevating arbitrary async methods into dangerous synchronous ones.

## 5. Conclusion for Security Researchers

`sync_call_restrictions.h` is a lynchpin of Mojo IPC security. It codifies a critical security policy: "Don't use sync IPC." The history of this file, particularly the re-entrancy bugs, serves as a powerful case study in the dangers of unexpected state changes during complex operations.

When auditing, pay close attention to:

*   **Any proposed changes to the `friend` allowlist.** This is the most sensitive part of the file.
*   **The context of `ScopedAllowSyncCall` usage.** Even with re-entrancy disabled, these locations are performing synchronous IPC and warrant scrutiny.
*   **The potential for deadlocks.** While disabling interrupts prevents re-entrancy, it increases the risk of deadlocks. The trade-off made here was to favor crashing safely over being exploited.

Understanding this component is essential for any security researcher analyzing the Chromium architecture, as it sits at the heart of the browser's defense against a potent class of IPC vulnerabilities.