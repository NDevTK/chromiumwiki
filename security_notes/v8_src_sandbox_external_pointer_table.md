# Security Analysis of `external-pointer-table.h`

This document provides a security analysis of the `external-pointer-table.h` file, which defines the interface for the V8 Sandbox's External Pointer Table (EPT). This component is a cornerstone of the sandbox's security, providing a mechanism to safely reference memory outside the V8 heap from within the sandboxed environment.

## 1. Core Security Design: Indirection as a Primitive

The fundamental security principle of the EPT is **indirection**. Instead of storing raw, 64-bit pointers on the V8 heap (which, if corrupted, could point anywhere in the process's memory), objects on the heap store a 32-bit `ExternalPointerHandle`. This handle is effectively an index into the EPT.

- **Security Implication:** This design is the **primary defense against sandbox escapes via pointer corruption**. An attacker who gains arbitrary read/write within the V8 heap can overwrite a handle, but they can only make it point to another entry *within the EPT*. They cannot use it to craft a pointer to an arbitrary address (e.g., the stack, GOT/PLT, or other sensitive data structures). At worst, a corrupted handle will cause a type confusion between two different kinds of external objects, or it will point to an invalid entry, both of which are significantly harder to exploit for a full sandbox escape than a direct arbitrary read/write primitive.

## 2. Type Safety via Tagging

The EPT enforces a form of type safety for external pointers, which is a critical second layer of defense.

- **`ExternalPointerTag`:** When a pointer is stored in the table (`Set`), it is "tagged" by being OR'd with a type-specific `ExternalPointerTag`. When the pointer is retrieved (`Get`), it is untagged.
- **`GetExternalPointer(ExternalPointerTagRange tag_range)`:** The `Get` operation requires the caller to specify the expected tag (or a range of valid tags). The implementation checks that the tag stored in the entry matches the expected tag.
- **Security Implication:** This tagging mechanism prevents an attacker from using a corrupted handle to cause a type confusion that would be useful for an exploit. For example, if an attacker corrupts a handle that is supposed to point to a `JSArrayBuffer` backing store and makes it point to an entry for a `JSFunction`'s native code object, the tag check will fail upon access. This will result in an invalid pointer being returned, leading to a safe crash rather than allowing the attacker to execute arbitrary code or read/write out of bounds.

## 3. Temporal Safety via Garbage Collection

The EPT is tightly integrated with V8's garbage collector to prevent use-after-free vulnerabilities involving external objects.

- **Marking Bit:** Each entry contains a marking bit. When an object on the V8 heap is marked as live during a GC cycle, the `Mark` function (line 412) is called for its corresponding EPT entry. This sets the marking bit in the table.
- **Sweeping:** After marking is complete, the `SweepAndCompact` function iterates through the table. Any entry that was not marked is considered dead and is reclaimed (added to a freelist). This ensures that entries for garbage objects are invalidated, preventing dangling pointers.
- **`ManagedResource`:** This class provides a mechanism for external C++ objects to tie their lifetime to the EPT. When a `ManagedResource` is destroyed, its destructor is expected to call `ZapExternalPointerTableEntry`, which invalidates the corresponding entry in the table, preventing it from becoming a dangling pointer.

## 4. Compaction and Memory Management

- **`EvacuateAndSweepAndCompact`:** The table supports compaction, which involves moving live entries to fill the gaps left by dead ones. This is a highly complex process that requires updating all corresponding `ExternalPointerHandle` values on the V8 heap.
- **Security Implication:** The complexity of the compaction algorithm is a potential source of vulnerabilities. A bug in this logic—for example, failing to update a handle on the heap after its entry has been moved, or an error in the freelist management—could lead to a handle pointing to the wrong entry, resulting in a type confusion or a dangling pointer that an attacker could exploit.

## Summary of Potential Security Concerns

1.  **Completeness:** The security of the EPT model depends on the **absolute absence** of raw, non-sandboxed pointers to external objects on the V8 heap. Any single raw pointer that is overlooked and not migrated to this system represents a potential hole in the sandbox.
2.  **Compaction Logic:** The memory compaction algorithm is extremely complex. A bug in this code could lead to state corruption within the EPT, potentially creating exploitable dangling pointers or type confusions.
3.  **Tagging Discipline:** The security of the type-tagging system relies on developers correctly assigning unique and appropriate tags to different types of external pointers. Reusing tags or failing to check the tag on access could undermine the type safety guarantees.
4.  **Lifetime of `ManagedResource`:** The `ManagedResource` pattern relies on the C++ object's destructor correctly zapping its EPT entry. A bug where an external object is freed without its entry being zapped would create a classic dangling pointer vulnerability.