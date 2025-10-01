# MimeSniffer (`net/base/mime_sniffer.cc`)

## 1. Summary

The `MimeSniffer` is a component responsible for guessing the MIME type of a resource by inspecting its content. This is a legacy feature, maintained for compatibility with websites that provide incorrect or missing `Content-Type` headers.

The component's own documentation explicitly warns against expanding its capabilities, stating: **"MIME sniffing only continues to be supported because of the many sites that depend on the existing behavior, not because it's a good idea."** It is considered a significant security risk.

## 2. Core Concepts

MIME sniffing logic operates under a specific set of rules to decide if it should override the server-provided `Content-Type` header.

*   **When to Sniff:** The decision to sniff is made by `ShouldSniffMimeType`. Sniffing is generally triggered if:
    *   The server provides a vague MIME type (e.g., `text/plain`, `application/octet-stream`).
    *   The server provides no MIME type at all.
    *   The URL has a `file://` scheme (under certain conditions).
    *   It specifically avoids sniffing types that are already "safe" and unambiguous (e.g., `image/png`).

*   **How to Sniff:** `SniffMimeType` examines the first `kMaxBytesToSniff` (1024) bytes of the response body. It looks for "magic bytes" or patterns characteristic of known file types (e.g., `GIF89a` for GIFs, `<html>` for HTML).

## 3. Security-Critical Logic & Vulnerabilities

MIME sniffing is inherently dangerous because it can lead to the browser interpreting a resource as a different type than the server intended. This is the foundation for several types of attacks.

*   **MIME Confusion Attacks / Content-Type Sniffing:**
    *   **The Core Problem:** An attacker uploads a file that is crafted to be a valid, non-executable type (like an `image/gif`) but also contains executable script content (like HTML/JavaScript).
    *   **The Attack:** The attacker tricks the browser into requesting this resource. If the MIME sniffer misidentifies the file as `text/html`, the browser will render it as a webpage, executing the embedded script in the context of the victim's origin.
    *   **Source Code Evidence (`mime_sniffer.cc:179`):** The code contains a comment explicitly acknowledging this risk:
        ```cpp
        // ...This could be a security
        // vulnerability if the site allows users to upload content.
        ```
        This comment refers to a check that prevents sniffing `text/plain` as `text/html` if the content contains a `<script>` tag but lacks other HTML tags, as a partial mitigation.

*   **Cross-Site Scripting (XSS):** MIME sniffing is a primary vector for XSS on sites that host user-generated content. If an attacker can upload a file that gets sniffed as HTML, they can execute arbitrary JavaScript.

*   **The `file://` Scheme:** The `ForceSniffFileUrlsForHtml` option is particularly dangerous. If enabled, it instructs the sniffer to aggressively check local files for HTML content. This could allow a malicious local file, perhaps disguised as a text or image file, to be opened and executed as HTML, potentially gaining access to other local files. The recommended and default setting is `kDisabled`.

## 4. Key Functions

*   `bool ShouldSniffMimeType(const GURL& url, std::string_view mime_type)`:
    *   The gatekeeper function that decides *if* sniffing should even be attempted based on the URL and the server-provided `Content-Type`.

*   `bool SniffMimeType(...)`:
    *   The main function that performs the content analysis. It's a large state machine that checks for patterns of many different file types, from images and media to ZIP archives and scripts. Its complexity is a source of risk.

*   `bool LooksLikeBinary(std::string_view content)`:
    *   A helper that checks for the presence of non-text control characters. This is used to differentiate between text-based formats (like HTML, JS) and binary formats.

## 5. Mitigations & Defenses

*   **For Site Owners:** The best defense is to always serve resources with a correct and unambiguous `Content-Type` header and, most importantly, the `X-Content-Type-Options: nosniff` header. This header explicitly tells the browser to disable MIME sniffing and trust the provided `Content-Type`.
*   **In Chromium:** The sniffing logic contains numerous heuristics to avoid the most blatant security issues, but it cannot be made completely safe. The code is intentionally kept frozen to avoid introducing new attack vectors.

## 6. Related Files

*   `net/base/mime_sniffer_unittest.cc`: Contains tests for the sniffing logic, which can be a good source for understanding the specific patterns it looks for.