# Windows Process Mitigations (`sandbox/win/src/process_mitigations.h`)

## 1. Summary

This component is the heart of Chromium's exploit mitigation strategy on Windows. It provides a framework for applying and managing various OS-level security policies to both the main browser process and, more importantly, to sandboxed child processes (like renderers).

These mitigations are a critical layer of defense-in-depth. They don't fix specific bugs but make entire classes of vulnerabilities (e.g., buffer overflows, use-after-frees) significantly harder or impossible to exploit successfully.

## 2. Core Concepts & Strategy

The framework employs a sophisticated, phased approach to applying security policies, recognizing that different processes have different needs and that privileges can be reduced over time.

*   **Browser Process Ratcheting:**
    *   The browser process starts with a minimal set of mitigations to allow for necessary initialization (`SetStartingMitigations`).
    *   As it starts up and no longer needs certain privileges (like writing to arbitrary parts of the registry or filesystem), it calls `RatchetDownSecurityMitigations` one or more times.
    *   This "ratcheting" model progressively tightens the security policy, adhering to the principle of least privilege by ensuring the process only has the access it needs at any given moment.

*   **Child Process Lockdown:**
    *   Child processes, which handle untrusted content, are born into a much stricter environment.
    *   `LockDownSecurityMitigations` is called once to apply a stringent, pre-defined set of mitigations from the very beginning of the process's lifetime.

*   **Timing of Application:**
    *   **Pre-Startup:** Most mitigations are applied before a process's main thread begins execution, often by configuring attributes on the process handle (`ApplyProcessMitigationsToSuspendedProcess`).
    *   **Post-Startup:** Some mitigations can only be enabled after the process is running (`CanSetProcessMitigationsPostStartup`).
    *   **Per-Thread:** Certain policies can even be applied on a per-thread basis (`ApplyMitigationsToCurrentThread`).

## 3. Key Security Mitigations Controlled

This framework is used to enable a wide array of Windows security features. The `MitigationFlags` enum represents these policies. Analysis of the source (`sandbox_policy_win.cc`) shows it controls:

*   **Memory Protections:**
    *   `MITIGATION_DEP` / `MITIGATION_DEP_NO_ATL_THUNK`: Enforces Data Execution Prevention (DEP) to prevent code execution from non-executable memory regions.
    *   `MITIGATION_HEAP_TERMINATE`: Causes the process to terminate immediately on heap corruption.
    *   `MITIGATION_BOTTOM_UP_ASLR`: Enforces mandatory bottom-up Address Space Layout Randomization (ASLR), making memory corruption exploits less reliable.

*   **Code Integrity & Flow Control:**
    *   `MITIGATION_STRICT_HANDLE_CHECKS`: Prevents the use of invalid handles.
    *   `MITIGATION_EXTENSION_POINT_DISABLE`: Disables legacy extension points (like AppInit_DLLs) that are common malware persistence mechanisms.
    *   `MITIGATION_CONTROL_FLOW_GUARD`: Enables Control Flow Guard (CFG) to prevent exploits from redirecting execution to illegitimate locations.
    *   `MITIGATION_RESTRICT_INDIRECT_BRANCH_PREDICTION`: Mitigates Spectre-style vulnerabilities.

*   **Filesystem & Font Security:**
    *   `MITIGATION_NONSYSTEM_FONT_DISABLE`: Prevents loading of untrusted, non-system fonts, a common attack vector in the kernel.
    *   `MITIGATION_IMAGE_LOAD_NO_REMOTE` / `NO_LOW_LABEL`: Restricts where executable images (DLLs) can be loaded from.

## 4. Security Considerations

*   **Defense-in-Depth:** The entire purpose of this file is defense-in-depth. A failure or bypass of one of these mitigations doesn't create a vulnerability, but it makes an existing vulnerability in the renderer or another component much more dangerous.
*   **Policy Gaps:** The most significant risk is a flaw in the logic that results in a mitigation *not* being applied when it should be. For example, a bug that prevents `LockDownSecurityMitigations` from being called on a renderer process would severely weaken the sandbox.
*   **Configuration Complexity:** The system is complex, with flags needing to be converted into different Windows API structures (`PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY`, `COMPONENT_FILTER`). An error in this translation could cause a policy to fail silently.
*   **OS Version Dependency:** The availability and behavior of these mitigations are highly dependent on the version of Windows. The code is full of checks to ensure policies are only applied on supported OS versions.

## 5. Related Files

*   `sandbox/win/src/process_mitigations.cc`: The concrete implementation of the functions declared in the header.
*   `sandbox/win/src/security_level.h`: Defines the `MitigationFlags` enum, which is the API for specifying which mitigations to apply.
*   `sandbox/policy/win/sandbox_win.cc`: The "client" of this framework. It defines the actual mitigation policies for different sandbox types (e.g., renderer, GPU) and calls the functions in this file to apply them.