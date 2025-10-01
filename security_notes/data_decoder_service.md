# Data Decoder Service (`services/data_decoder/data_decoder_service.h`)

## 1. Summary

The `DataDecoderService` is a dedicated, sandboxed utility process responsible for parsing untrusted data on behalf of more privileged processes like the browser process. Its sole purpose is to act as a "jail" for complex and historically vulnerability-prone data parsing libraries. By moving this parsing off of the main browser process, a vulnerability in a parser (e.g., a heap overflow in an image decoder) is contained within a highly restrictive sandbox and cannot be used to directly compromise the browser or the underlying operating system.

This service is a textbook example of applying the principle of least privilege and defense-in-depth. It is a critical component of Chromium's security architecture for mitigating the risks associated with processing complex, untrusted file formats.

## 2. Core Concepts

*   **Sandboxed Service:** The key security property of the `DataDecoderService` is that it runs in one of the most restrictive sandboxes in Chromium. It has no access to the filesystem, network, user interface, or most system calls. Its only capability is to receive data over a Mojo IPC channel, parse it, and return the result (or an error) over the same channel.

*   **Specialized Parsers:** The service is a host for several distinct parsing capabilities, each exposed as a separate Mojo interface. As seen in the header, these include:
    *   `mojom::ImageDecoder`: For parsing and decoding common image formats (PNG, JPEG, etc.).
    *   `mojom::XmlParser`: For parsing XML documents.
    *   `mojom::CborParser`: For parsing CBOR (Concise Binary Object Representation) data.
    *   `web_package::mojom::WebBundleParserFactory`: For parsing Web Bundles.
    *   And others like `Gzipper` and `StructuredHeadersParser`.

*   **On-Demand Launch:** The service is typically launched on-demand by the `DataDecoderService` launcher in the browser process when a client first requests one of its interfaces. It can be set to terminate itself after being idle to conserve resources.

## 3. Security-Critical Logic & Vulnerabilities

The security of the `DataDecoderService` does not come from complex logic within the service itself, but from the **strength of the sandbox** that contains it.

*   **The Sandbox as the Primary Defense:**
    *   **Risk:** The single greatest risk is a memory corruption vulnerability (e.g., buffer overflow, use-after-free, integer overflow) in one of the underlying parsing libraries (e.g., `libxml`, `libpng`). An attacker can craft a malicious file to trigger such a bug.
    *   **Mitigation:** The sandbox is the *only* thing that prevents such a bug from becoming a full remote code execution vulnerability. The sandbox policy for this service is extremely restrictive, denying access to almost all kernel syscalls except for those absolutely necessary for memory allocation and basic computation. The security of the entire model depends on the integrity of this sandbox.

*   **Interface Definition as Attack Surface:**
    *   **Risk:** Each `Bind...` method in the `DataDecoderService` interface (e.g., `BindImageDecoder`) defines an entry point through which untrusted data can be fed into the sandboxed process. A bug in the Mojo deserialization layer itself, or a logic bug in how the service handles a new binding, could be a potential vulnerability.
    *   **Mitigation:** The attack surface is intentionally kept minimal. The service only exposes methods to bind receivers for specific, well-defined parsing tasks. It does not expose any generic or privileged capabilities.

*   **Sandbox Configuration:**
    *   **Risk:** The most critical potential vulnerability would be a configuration error in the browser process that causes the `DataDecoderService` to be launched *without* its sandbox. This would be a critical failure of the entire security model, as a bug in a parser would then execute with the full privileges of the browser process.
    *   **Mitigation:** The logic for launching utility processes (in `content/browser/utility_process_host.cc`) is responsible for correctly applying the `sandbox::policy::SandboxType::kDataDecoder` policy.

*   **Missing JSON Parser:** Notably, the `DataDecoderService` does **not** expose a `JsonParser` interface. This is a deliberate security design choice. JSON parsing is handled by a separate, dedicated service (`//services/data_decoder/public/cpp/json_parser.h`). This further compartmentalizes risk; a vulnerability in the XML parser does not affect the JSON parser, and vice-versa.

## 4. Related Files

*   `services/data_decoder/data_decoder_service.cc`: The implementation of the service, which primarily consists of boilerplate for binding the various parser implementations.
*   `content/browser/utility_process_host.cc`: The code in the browser process responsible for launching utility processes, including the `DataDecoderService`, and applying the correct sandbox policy to them.
*   `sandbox/policy/linux/bpf_utility_policy_linux.cc`: An example of the actual seccomp-bpf sandbox policy applied to utility processes like the data decoder on Linux. It is extremely restrictive.
*   The individual parser implementations (`image_decoder_impl.cc`, `xml_parser.cc`, etc.), which contain the complex and vulnerability-prone parsing logic.