# V8 Sandbox (`v8/src/sandbox/sandbox.h`)

## 1. Summary

The V8 Sandbox is a foundational, high-impact security feature designed to mitigate memory corruption vulnerabilities within the JavaScript engine. It functions as a virtual memory "cage," reserving a massive, contiguous region of address space. The vast majority of V8 objects, including the JavaScript heap and ArrayBuffer backing stores, are allocated exclusively within this sandbox.

The primary goal is **containment**. By controlling how memory is addressed both within and outside this boundary, the sandbox aims to make it impossible for an attacker who has achieved arbitrary read/write *inside* the sandbox to corrupt memory or execute code *outside* the sandbox, thereby preventing a JIT engine bug from escalating into a full renderer compromise.

## 2. Core Security Mechanisms

The sandbox's security relies on several interconnected mechanisms:

*   **Sandboxed Pointers:**
    *   **Concept:** Within the sandbox, direct raw pointers are forbidden for references between sandboxed objects. Instead, V8 uses **32-bit offsets** relative to the sandbox's `base()` address.
    *   **Security Impact:** This is the cornerstone of the sandbox. If an attacker gains the ability to corrupt a pointer, they can only change its offset. This means they can make the pointer point to any other location *within* the sandbox's bounds, but they cannot forge a pointer to an arbitrary address outside the sandbox (e.g., to the process's stack, GOT/PLT tables, or other executable memory). This contains the corruption.

*   **External Pointer Table:**
    *   **Concept:** To reference memory *outside* the sandbox (e.g., pointers to C++ objects in Blink or the C++ standard library), V8 uses an **External Pointer Table (EPT)**. Sandboxed objects do not store raw external pointers; they store a 32-bit handle which is an index into the EPT.
    *   **Security Impact:** The EPT acts as a secure indirection layer. The table itself is located outside the sandbox and is protected. When V8 needs to dereference an external pointer, it uses the handle to look up the real pointer in the table. An attacker who corrupts a handle can only change the index, making it point to another valid, trusted entry in the EPT. They cannot make it point to arbitrary memory. This prevents an attacker from using a corrupted object to pivot to other parts of the process's memory.

*   **Guard Regions:**
    *   **Concept:** In its ideal configuration, the sandbox's vast memory reservation is surrounded by inaccessible guard pages.
    *   **Security Impact:** This prevents linear overflow/underflow attacks where an attacker with a corrupted pointer might try to perform a small out-of-bounds access (e.g., `pointer[index]`) to escape the sandbox. The guard pages ensure such an access will immediately crash the process rather than corrupting adjacent memory.

*   **Smi Address Range Protection:**
    *   **Concept:** V8 attempts to reserve the first 4GB of the process's address space and leave it inaccessible.
    *   **Security Impact:** This mitigates a class of bugs known as "Smi-to-HeapObject confusion." In V8, small integers (Smis) are specially tagged. If a bug causes a Smi to be misinterpreted as a pointer to a heap object, the low numerical value would be dereferenced as an address. By making the entire low-address range un-mappable, this type of bug leads to a safe, immediate crash instead of a potential exploit primitive.

## 3. Security Considerations & Weaknesses

*   **Partially-Reserved Sandbox Fallback:**
    *   The most significant security trade-off is the "partially-reserved sandbox." On systems with limited virtual address space, V8 may not be able to reserve the entire desired sandbox region.
    *   The `is_partially_reserved()` method indicates when this weaker mode is active. In this configuration, the sandbox's security guarantees are reduced because unrelated memory mappings could end up inside the sandbox's address range, potentially providing a target for a corruption vulnerability.

*   **Initialization Integrity:** The entire security model depends on the `Initialize()` method correctly setting up the memory layout, `base` address, and guard pages. A flaw in initialization could weaken or disable the sandbox entirely.

*   **Data vs. Control Flow:** The sandbox primarily protects against data corruption and direct control-flow hijacking (by preventing pointers to code). It does not, by itself, protect against logic bugs or side-channel attacks (like Spectre), although it makes exploiting them much harder.

## 4. Key Functions and Members

*   `base()`: Returns the start `Address` of the sandbox. This is the single most important value, as all sandboxed pointer calculations are relative to it.
*   `Contains(Address addr)`: The canonical way to check if a given address is within the sandbox's boundaries.
*   `page_allocator()`: Exposes a `PageAllocator` that allows other parts of Chromium (like Blink, for ArrayBuffers) to allocate memory *inside* the sandbox, ensuring that potentially large, script-accessible buffers are also contained.
*   `is_partially_reserved() const`: A critical function for determining the security posture of the sandbox. Returns true if the sandbox is operating in its less-secure fallback mode.

## 5. Related Files

*   `v8/src/sandbox/sandbox.cc`: The implementation of the sandbox logic.
*   `v8/src/sandbox/external-pointer.h`: Defines the External Pointer Table (EPT) and the logic for securely referencing memory outside the sandbox.
*   `v8/src/objects/heap-object.h`: Contains the definitions for heap objects and how they store sandboxed pointers vs. raw pointers.
*   `v8/src/api/api.cc`: The public V8 API, which includes functions for interacting with security-related concepts like the `SecurityToken`.