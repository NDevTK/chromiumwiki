# MP4 BoxReader (`media/formats/mp4/box_reader.h`)

## 1. Summary

The `BoxReader` is a low-level, security-critical class responsible for parsing the fundamental structure of an MP4 container file. The MP4 format is composed of a series of nested "boxes" (or "atoms"), each with a type and a size. The `BoxReader` provides the core functionality to read a box's header, identify its type and size, and then recursively parse the sub-boxes contained within it.

This class sits at the very beginning of the media parsing pipeline. Nearly every other MP4 parsing component (e.g., for video tracks, audio tracks, or metadata) relies on the `BoxReader` to correctly delimit the data they are supposed to parse. A vulnerability in the `BoxReader`, such as an integer overflow in a size calculation or an incorrect bounds check, could lead to a heap overflow or other memory corruption in a more privileged process (like the browser process, if not using a dedicated media utility process), which could lead to remote code execution.

## 2. Core Concepts

*   **Box Structure:** An MP4 file is a tree of boxes. Each box starts with a header containing its size (32 or 64 bits) and its type (a 4-byte "FourCC" code). The `BoxReader` is responsible for reading this header.

*   **Recursive Parsing:** The `BoxReader` is designed for recursion. After reading a parent box, a new `BoxReader` can be created for that box's contents to parse its children. The `ScanChildren()` method populates an internal map of all immediate child boxes, which can then be parsed by higher-level logic.

*   **Bounds Checking:** The `BoxReader`'s most critical responsibility is to ensure that all parsing operations remain strictly within the bounds of the current box's declared size. It must never read past the end of its buffer or the end of the box it's supposed to be parsing.

*   **`RCHECK` Macro:** The implementation heavily uses a custom macro `RCHECK` (`rcheck.h`). This is a specialized `DCHECK` that is designed to handle parsing of untrusted data. If an `RCHECK` fails, it immediately stops parsing and returns an error, preventing further processing of the malformed data.

## 3. Security-Critical Logic & Vulnerabilities

The entire class is security-critical, as it is the foundation for all other MP4 parsing.

*   **Integer Overflows in Size Calculation:**
    *   **Risk:** The size of a box is read from the file as a 32-bit or 64-bit integer. An attacker could craft a file with a malicious, extremely large size value. If this value is used in calculations without being checked for overflow, it could lead to an undersized buffer being allocated, followed by a heap overflow when the parser attempts to copy the box's contents.
    *   **Mitigation:** The `ReadHeader` method must carefully validate the box size. It checks if the size is smaller than the header itself and ensures that the remaining buffer is large enough to contain the declared size. The `HasBytes` method also contains a check against `kImplLimit` to prevent unreasonable read requests.

*   **Incorrect Bounds Enforcement:**
    *   **Risk:** If the `BoxReader` incorrectly calculates its own boundaries or the boundaries of a child box, it could allow a sub-parser to read or write out of bounds. For example, if `ScanChildren` miscalculates the offset of a child box, it could pass an incorrect pointer to a later parsing stage.
    *   **Mitigation:** The `pos_` member tracks the current position within the buffer. Every read operation must first call `HasBytes()` to validate that the read is within the available data. The logic in `ReadAllChildrenInternal` carefully increments `pos_` by the size of each child box it reads, ensuring it does not advance past the end of the parent box.

*   **Recursive Depth and Complexity:**
    *   **Risk:** An attacker could craft a file with an extremely deep nesting of boxes. If the parsing logic is purely recursive, this could lead to a stack exhaustion denial-of-service attack.
    *   **Mitigation:** The `ScanChildren` method mitigates this by only scanning one level at a time. It populates a `ChildMap` of the immediate children, but does not recursively scan them. This makes the parsing iterative rather than deeply recursive, which is a much safer pattern for handling untrusted data.

## 4. Key Functions

*   `BoxReader::ReadTopLevelBox(...)` / `StartTopLevelBox(...)`: Static methods that are the entry point for parsing a new MP4 file from the top. They perform initial validation.
*   `ReadHeader()`: The private method that reads the `size` and `type` from the bytestream. This is a primary validation point.
*   `HasBytes(size_t count)`: The fundamental bounds-checking method. It must be called before every read.
*   `ScanChildren()`: Populates the internal map of child boxes, enabling iterative parsing of the box hierarchy.
*   `ReadChild(...)` / `ReadChildren(...)`: The methods used by higher-level parsers to request and parse specific, expected child boxes.

## 5. Related Files

*   `media/formats/mp4/box_definitions.cc`: Contains the implementations of the `Parse` methods for all the specific box types (`moov`, `trak`, `avc1`, etc.). All of these classes use a `BoxReader` to do their work.
*   `media/formats/mp4/mp4_stream_parser.cc`: A higher-level parser that uses `BoxReader` to parse an MP4 file from a stream.
*   `media/formats/mp4/rcheck.h`: Defines the `RCHECK` macro, which is the primary error-handling mechanism for the parsers.