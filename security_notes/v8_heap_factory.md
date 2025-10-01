# V8 Heap Factory (`v8/src/heap/factory.cc`)

## 1. Summary

The `Factory` class is the central, unified interface for allocating and initializing all objects on the V8 managed heap. It is the sole "constructor" for every object type, from simple numbers and strings to complex `JSFunction`s, `Map`s (hidden classes), and `JSTypedArray`s.

Because it is responsible for creating every object and ensuring its initial state is valid and consistent, the `Factory` is one of the most security-critical components in V8. A single logical error in a factory method can undermine the type safety and memory integrity of the entire JavaScript engine, often leading to severe vulnerabilities.

## 2. Core Concepts

*   **Centralized Allocation:** All heap object allocations are routed through the `Factory`. It abstracts away the details of which memory space (e.g., New Space, Old Space, Read-Only Space) an object should be placed in.

*   **Initialization Guarantee:** The factory's primary responsibility is not just to allocate memory, but to ensure that the allocated object is correctly initialized before it is returned to the caller. This includes:
    *   Setting the object's `Map` (its hidden class).
    *   Initializing all fields to a valid default state (e.g., filling with `the_hole_value`, setting lengths to zero).
    *   Linking objects into necessary lists (e.g., adding a new `AllocationSite` to the global list).

*   **Canonical Objects:** The factory provides access to canonical, immutable objects like `empty_string()`, `nan_value()`, and various root maps.

*   **String Interning:** It manages the string table, ensuring that identical strings are represented by a single, unique `InternalizedString` object. This is important for both performance (property lookups) and security (preventing issues with duplicate but distinct string objects).

## 3. Security-Critical Logic & Vulnerabilities

The `Factory` is a primary location for enforcing security invariants and hardening the engine against exploitation.

*   **Correct Object Initialization:**
    *   **Risk:** The most significant risk is a factory method returning a partially or incorrectly initialized object. If a field that is supposed to contain a length is left uninitialized, it could contain a garbage value that allows out-of-bounds reads/writes. If a pointer field is not initialized, it could be exploited for type confusion.
    *   **Mitigation:** The factory methods are carefully constructed to ensure all fields are set before returning. The use of `DisallowGarbageCollection` scopes during initialization is common to prevent a GC from observing a partially-initialized object.

*   **TypedArray/Resizable Buffer Security:**
    *   **Risk:** Resizable `ArrayBuffer`s introduce a significant attack surface. If a `TypedArray` or `DataView` that points to a resizable buffer does not correctly track the buffer's changing size, it can retain a stale length, leading to out-of-bounds access.
    *   **Mitigation & Source Code Evidence:** The factory contains explicit security checks to mitigate this. When creating a "length-tracking" view (one that is attached to a resizable buffer), it forces the initial length to zero, ensuring the view cannot be used until it is properly configured.
        *   In `NewJSTypedArray`:
            ```cpp
            // v8/src/heap/factory.cc:3745
            if (is_length_tracking) {
              // Security: enforce the invariant that length-tracking TypedArrays have
              // their length and byte_length set to 0.
              length = 0;
            }
            ```
        *   In `NewJSDataViewOrRabGsabDataView`:
            ```cpp
            // v8/src/heap/factory.cc:3768
            if (is_length_tracking) {
              // Security: enforce the invariant that length-tracking DataViews have their
              // byte_length set to 0.
              byte_length = 0;
            }
            ```
    *   A failure to enforce these invariants would be a critical vulnerability.

*   **Map Integrity:** The factory is responsible for assigning the correct `Map` to a new object. A bug that assigns a `Map` for a `JSArray` to a `JSObject` would be a classic type confusion primitive, allowing an attacker to manipulate object fields as if they were array elements, leading to arbitrary memory read/write.

## 4. Key Functions

*   `New(...)`: The fundamental method for allocating a new object and setting its map.
*   `NewJSTypedArray(...)`: Creates a new `JSTypedArray`, containing the critical security check for length-tracking arrays.
*   `NewJSArrayBuffer(...)`: Creates `ArrayBuffer`s, the underlying storage for typed arrays.
*   `NewMapImpl(...)`: The core implementation for creating new `Map`s (hidden classes), which defines the shape and behavior of all future objects.
*   `InternalizeString(...)`: The entry point to the string table, crucial for ensuring string uniqueness.

## 5. Related Files

*   `v8/src/heap/heap.h`: The `Heap` class, which manages the actual memory spaces and is used by the `Factory` to perform allocations.
*   `v8/src/objects/objects.h`, `v8/src/objects/js-array-buffer.h`: Define the in-memory layouts of the objects that the `Factory` constructs. The factory must correctly adhere to these layouts.
*   `v8/src/objects/map.h`: Defines the `Map` class. The factory's role in correctly creating and assigning maps is fundamental to V8's type system and security.