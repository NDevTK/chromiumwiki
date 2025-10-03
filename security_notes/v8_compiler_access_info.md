# Security Notes: `v8/src/compiler/access-info.cc`

## File Overview

This file is arguably one of the most security-critical components in the entire V8 engine. It is the intelligence hub for V8's optimizing JIT compiler, TurboFan. Its primary responsibility is to analyze JavaScript property accesses (e.g., `obj.prop`) and provide the compiler with precise information—`PropertyAccessInfo`—about how to perform that access.

Based on this information, the JIT generates highly specialized and fast machine code. The security of the entire JIT compilation process depends on the absolute correctness of the information computed by this file. An error here can lead to the JIT generating code that makes incorrect assumptions, resulting in type confusion vulnerabilities, which are the most common and severe class of bugs in JavaScript engines.

## The Core Security Challenge: Preventing Type Confusion

The fundamental goal of this file is to allow for optimization without introducing security holes. The JIT wants to replace a generic property access with a direct memory load from a fixed offset. To do this safely, it must be certain about the object's structure (its "map" or "shape").

-   **The Optimization**: `obj.x` becomes `mov rax, [rdi + <offset_of_x>]`.
-   **The Risk**: If the JIT compiler generates this code based on a faulty assumption that `obj` has a map where `x` is at `offset_of_x`, but at runtime `obj` has a different map, this instruction will read or write memory out-of-bounds. This can lead to information leaks or, more commonly, attacker-controlled object corruption, which is a powerful primitive for achieving arbitrary code execution.

This file's entire purpose is to provide the guarantees needed to prevent this scenario.

## Key Security Mechanisms and Concepts

### 1. `CompilationDependencies`: The Safety Net

This is the most important security mechanism related to JIT compilation. The `AccessInfoFactory` makes optimistic assumptions, and for every assumption it makes, it must register a `CompilationDependency`. If that assumption is violated later at runtime (e.g., a prototype is modified, a field changes its representation), the dependency is triggered, and the JIT-compiled code is immediately thrown away ("deoptimized"). Execution safely falls back to the slower interpreter.

-   **`unrecorded_dependencies_`**: `PropertyAccessInfo` objects collect dependencies here. They are not "live" until they are explicitly registered with the `CompilationDependencies` object via `RecordDependencies()`.
-   **Types of Dependencies**:
    -   **`DependOnStableMap`**: Assumes an object's map will not change.
    -   **`DependOnStablePrototypeChain`**: Assumes the prototype chain of an object will not change.
    -   **`FieldRepresentationDependency`**: Assumes a field will continue to hold a specific type of value (e.g., a Smi or a Double).
    -   **`FieldConstnessDependency`**: Assumes a property marked as `const` will not be written to.

A missing dependency for a given assumption is a critical vulnerability.

### 2. `CanInlinePropertyAccess`: The First Line of Defense

This function acts as a gatekeeper, deciding if a property access is even a candidate for JIT optimization. It correctly identifies and bails out on several complex or insecure scenarios:

-   `!map.object()->has_named_interceptor()`: Named interceptors are used for DOM objects where property access is handled by complex C++ logic. The JIT cannot reason about this and must not try to optimize it.
-   `!map.is_access_check_needed()`: This is a critical security check for cross-origin access. Bypassing this would break the same-origin policy.
-   **Dictionary Mode**: It applies much stricter rules for objects in "dictionary mode" (which have a hash-table-like layout), correctly identifying them as harder to reason about statically.

### 3. The Prototype Chain Walk (`ComputePropertyAccessInfo`)

This is the main state machine that traverses the prototype chain to find where a property is defined. Its correctness is paramount.

-   **Starts with Receiver Map**: It begins with the map of the object receiving the property access.
-   **Handles Primitives**: Correctly finds the prototype for primitive types (e.g., getting the `String.prototype` for a string literal).
-   **Bails on Proxies**: It correctly identifies `JSProxy` objects on the prototype chain as a reason to invalidate the optimization, as their behavior is dynamic and unpredictable.
-   **Iterative Deepening**: It walks up the chain (`map = map.prototype(broker()).map(broker())`), checking for the property at each level.

### 4. Field Representation and Constantness

The code meticulously tracks and depends on the representation of data in a field.

-   **`details.representation()`**: It checks if a field is a `Smi`, `Double`, or `HeapObject`. An incorrect assumption here is a classic type confusion. For example, if the JIT believes a field is a `Double` and writes a 64-bit floating-point number, but the field actually contains a tagged pointer, the pointer will be corrupted, leading to memory corruption.
-   **`PropertyConstness::kConst`**: The factory identifies properties that are declared `const`. This allows the JIT to bake the constant value directly into the compiled code. However, it critically pairs this optimization with a `FieldConstnessDependency`. If JavaScript code somehow makes the field mutable later, the optimized code is safely discarded.

## Summary of Security Posture

`access-info.cc` is a battlefield where performance and security are in constant tension.

-   **Security Model**: Its security relies on a "speculate and verify" model. The compiler makes aggressive assumptions to generate fast code, but every assumption is backed by a `CompilationDependency` that acts as a tripwire.
-   **High-Impact Target**: This file is one of the most fruitful places to look for JIT vulnerabilities. A single logical error—a missed dependency, an incorrect assumption in the prototype walk, or a flawed merge of access information—can easily lead to a full remote code execution exploit.
-   **Areas for Audit**:
    -   **Missing Dependencies**: The most critical bug class. Does every assumption made in the file have a corresponding dependency registration?
    -   **Merge Logic (`Merge` function)**: When combining information from different code paths, can the merged `PropertyAccessInfo` represent an unsafe state that is less restrictive than the individual states?
    -   **Edge Case Objects**: How are proxies, modules, typed arrays, and objects with interceptors handled? These complex cases are where bugs often hide.