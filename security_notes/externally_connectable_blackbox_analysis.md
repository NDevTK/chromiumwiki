# Security Analysis: `externally_connectable` (Black-Box)

**Methodology:** This analysis is performed "black-box," without direct access to the Chromium source code. It is based on the official developer documentation and a foundational understanding of Chromium's security architecture. The goal is to hypothesize the necessary security controls and identify potential vulnerabilities based on the feature's intended behavior.

## 1. Feature Overview & Security Goal

The `externally_connectable` manifest key serves a single, critical security function: it acts as an **explicit, configurable gate** that defines which external entities (other extensions or regular web pages) are permitted to establish a communication channel (`runtime.connect`, `runtime.sendMessage`) with an extension.

Its primary security goal is to **prevent unauthorized websites or extensions from accessing an extension's APIs**. Since extensions often have powerful permissions (e.g., access to storage, cookies, web requests), a breach of this gate could allow a malicious web page to escalate its privileges to that of the extension.

## 2. Hypothesized Implementation & Key Security Checks

A secure implementation of this feature *must* perform the following checks within the trusted browser process. These checks would likely occur in a central IPC dispatcher (`ExtensionMessageFilter` or a modern Mojo equivalent) before a message or connection is routed to the target extension's `MessageService`.

1.  **Centralized Manifest Parsing**: The `externally_connectable` block in `manifest.json` must be parsed into a structured, in-memory representation (e.g., an `ExternallyConnectableInfo` object) that is immutably associated with the `Extension` object. This parsing step is the first line of defense.
    *   **Security Check:** It must **strictly reject** invalid match patterns during parsing, such as those containing wildcard domains (`*://*.google.com`) or subdomains of effective Top-Level Domains (eTLDs) (`*://*.co.uk`). Failure to reject invalid patterns at load time creates a latent vulnerability.

2.  **Authoritative Sender Identification**: When an IPC arrives to open a channel, the browser process must authoritatively identify the sender.
    *   **For web pages:** The sender's identity is its **security origin**, which must be retrieved from the trusted `RenderFrameHost` object. This identity must not be supplied by the potentially-compromised renderer itself.
    *   **For extensions:** The sender's identity is its extension ID, retrieved from the `RenderProcessHost`.

3.  **Strict Origin & ID Matching**: The core security logic compares the sender's identity against the parsed allowlist.
    *   **Web Page Check:** The sender's origin URL must be compared against the `matches` patterns. This comparison must be robust and correctly implemented according to URL specification standards. It must not be a simple string match.
    *   **Extension ID Check:** The sender's extension ID must be compared against the `ids` list. A wildcard `"*"` must be handled explicitly.

4.  **Secure Default Behavior**: The documentation states that if `externally_connectable` is not present, no web pages can connect. This is a **secure default** and is critical for the security of the vast majority of extensions that do not need external connectivity. The implementation must guarantee this default.

## 3. Attack Surface & Potential Vulnerabilities

The primary attack surface is the **parsing and matching of the `matches` URL patterns**. An attacker's goal is to find a way for their malicious website (`https://evil.com`) to be incorrectly matched by an allowlist entry for a trusted site (`https://trusted.com`).

### 3.1. High-Severity: Match Pattern Bypass

*   **Vulnerability:** A flaw in the URL pattern matching logic could allow an attacker-controlled origin to impersonate an allowed origin.
*   **Attack Scenarios:**
    1.  **eTLD Confusion:** The implementation fails to correctly use the Public Suffix List. An extension specifies `https://*.herokuapp.com` (which should be valid), but the matcher incorrectly allows `https://myapp.herokuapp.com.attacker.com` to connect.
    2.  **Path/Component Confusion:** The matcher is implemented with naive string functions. An extension allows `https://google.com/`, but the matcher incorrectly allows `https://google.com.evil.com/`. The matching logic must be correctly anchored to the hostname.
    3.  **Failure to Enforce Wildcard Ban:** The documentation forbids `*` in the domain part of a `matches` pattern. If the manifest parser fails to reject this, an extension could be tricked (e.g., via social engineering of the developer) into using a pattern like `*://*.com/*`, allowing *any* `.com` website to connect.

### 3.2. Medium-Severity: Race Conditions (TOCTOU)

*   **Vulnerability:** A Time-of-Check to Time-of-Use (TOCTOU) bug in the connection process.
*   **Attack Scenario:**
    1.  A web page at an allowed origin (`https://trusted.com`) initiates a connection via `runtime.connect()`.
    2.  The IPC is sent to the browser process. The browser checks the frame's origin, and it passes.
    3.  The attacker's script then rapidly navigates the frame to `https://evil.com`.
    4.  If the connection port is established and returned to the renderer *after* the navigation, the malicious page might gain control of a port that was approved for a different origin.
*   **Required Defense:** The browser must ensure that the port is either fully delivered to the original page's context before navigation can commit, or that the connection is torn down if a cross-origin navigation occurs before delivery.

### 3.3. Informational: Permissive Default for Extensions

*   **Issue:** By default, if `externally_connectable` is not specified, *all* other extensions can connect.
*   **Security Implication:** This creates a broad, implicit attack surface. An extension may not be designed with the expectation that every other extension on the system can probe its message-passing APIs. A malicious extension could be installed and then systematically attack other extensions, looking for internal APIs that can be exploited.
*   **Recommendation:** For security-sensitive extensions, developers should define an `externally_connectable` block with an empty `ids` array to ensure no other extensions can connect, adhering to the principle of least privilege.