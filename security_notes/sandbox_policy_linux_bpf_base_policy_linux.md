# Security Analysis of `sandbox/policy/linux/bpf_base_policy_linux.cc`

## Summary

`bpf_base_policy_linux.cc` is the foundational layer of Chromium's seccomp-bpf sandboxing architecture on Linux. It defines the `BPFBasePolicy` class, which serves as the common ancestor for all other, more specific BPF policies (e.g., for the renderer, GPU, or utility processes). Its primary security role is to establish a mandatory, non-negotiable "baseline" policy, ensuring that every sandboxed process adheres to a minimal, common set of security restrictions.

## The Core Security Principle: Inheritance and Delegation

The security model of `BPFBasePolicy` is elegantly simple and powerful. It does not, by itself, contain a large list of allowed or denied system calls. Instead, its core function is to instantiate and delegate to another class: `sandbox::BaselinePolicy`.

```cpp
// Constructor for BPFBasePolicy
BPFBasePolicy::BPFBasePolicy()
    : baseline_policy_(std::make_unique<BaselinePolicy>(kFSDeniedErrno)) {}

// Evaluation of a system call
ResultExpr BPFBasePolicy::EvaluateSyscall(int system_call_number) const {
  DCHECK(baseline_policy_);

  // ... (a minor special case for set_robust_list)

  return baseline_policy_->EvaluateSyscall(system_call_number);
}
```

This design has profound security implications:

1.  **Enforcing a Secure Default**: The `BaselinePolicy` (defined in `sandbox/linux/seccomp-bpf-helpers/baseline_policy.h`) implements the "default-deny" principle. It explicitly allows a very small set of system calls that are absolutely essential for any process to run (e.g., memory management like `mmap`, thread synchronization like `futex`, and process exit like `exit_group`). Any syscall not on this short list is killed by the kernel. By forcing all other policies to inherit from `BPFBasePolicy`, Chromium guarantees that no sandboxed process can ever be created without this fundamental "default-deny" protection.

2.  **Preventing Weak Policies**: This inheritance model prevents developers from accidentally or intentionally creating a weak sandbox policy. A developer creating a policy for a new utility process *must* inherit from `BPFBasePolicy` and therefore automatically gets the robust security guarantees of the baseline policy. They can only *add* permissions to this baseline; they cannot remove fundamental restrictions.

3.  **Layered Security (Defense-in-Depth)**: This architecture creates a clear security hierarchy:
    *   **Layer 0 (`BaselinePolicy`)**: The absolute minimum set of syscalls required for a process to exist. This is the most trusted and heavily scrutinized part of the policy.
    *   **Layer 1 (`BPFBasePolicy`)**: A thin wrapper that ensures the `BaselinePolicy` is always included.
    *   **Layer 2 (e.g., `RendererPolicy`, `GpuPolicy`)**: These policies build upon the base, adding only the specific, additional syscalls needed for their particular task. This adheres strictly to the Principle of Least Privilege.

## `kFSDeniedErrno = EPERM`

A small but important security detail is the choice of error code for denied filesystem operations:

```cpp
static const int kFSDeniedErrno = EPERM;
```

When a sandboxed process tries to make a forbidden filesystem syscall (like `open`), the BPF filter returns `EPERM` (Operation not permitted). This is a deliberate choice. Returning a consistent, expected error code prevents unexpected behavior in third-party libraries or application code that might try to handle different `errno` values in different ways. `EPERM` is the most semantically correct and least surprising error for a permissions-based denial.

## Conclusion

`BPFBasePolicy` is the bedrock of the seccomp-bpf sandbox. While the code itself is small, its architectural role is immense. It ensures that every sandboxed process in Chromium starts with a strong, default-deny security posture. By mandating the inclusion of the `BaselinePolicy`, it provides a powerful, consistent, and non-bypassable foundation for the entire sandboxing system, making it a cornerstone of the browser's defense against exploitation.