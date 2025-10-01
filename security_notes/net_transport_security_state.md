# TransportSecurityState (`net/http/transport_security_state.h`)

## 1. Summary

`TransportSecurityState` is a central class in Chromium's network stack responsible for enforcing policies that enhance transport layer security. It maintains the in-memory store for three primary security features:

1.  **HTTP Strict Transport Security (HSTS):** Forces connections to specific hosts to be upgraded from HTTP to HTTPS.
2.  **HTTP Public Key Pinning (HPKP):** (Now deprecated and largely disabled) Validates the public keys of a server's certificate chain against a set of "pinned" keys.
3.  **Certificate Transparency (CT):** (Partially managed here, but mostly in the cert verifier now) Enforces that certificates for certain hosts are publicly logged in a Certificate Transparency log.

This component is crucial for mitigating man-in-the-middle (MitM) attacks, particularly SSL stripping, and for ensuring the integrity of TLS connections.

## 2. Core Concepts

The state is managed in two forms:

*   **Dynamic State:** Learned at runtime from HTTP headers (`Strict-Transport-Security`, `Public-Key-Pins`). This state is perishable and stored in memory, with an optional `Delegate` to persist it across sessions.
*   **Static State (Preloading):** Compiled directly into Chromium from a static list (`transport_security_state_static.json`). This provides a baseline of protection for high-value sites, preventing attacks on the first visit before any dynamic headers have been seen. Dynamic state always overrides static state.

### HSTS (Strict Transport Security)

*   **Purpose:** To prevent SSL stripping attacks where an attacker downgrades an HTTPS connection to plain HTTP.
*   **Mechanism:** When a site sends an HSTS header, `TransportSecurityState` records that this domain (and optionally its subdomains) must only be accessed over HTTPS for a specified duration.
*   **Key Method:** `ShouldUpgradeToSSL()` checks if a given host has an active HSTS policy, returning true if the connection must be upgraded.

### HPKP (Public Key Pinning)

*   **Purpose:** To prevent MitM attacks involving fraudulently issued certificates.
*   **Mechanism:** A site provides a set of public key hashes (SPKIs) in a header. On subsequent connections, the browser verifies that at least one key from the server's certificate chain matches a key in the pinned set.
*   **Security Risk & Deprecation:** HPKP was found to be dangerous because a misconfiguration could lead to **"pinning suicide,"** where a site becomes completely inaccessible. Due to this risk, it has been deprecated in modern browsers, but the code remains.
*   **Key Method:** `CheckPublicKeyPins()` performs the validation of a certificate chain's keys against the stored pins.

## 3. Key Classes and Data Structures

*   **`STSState`:**
    *   Represents the HSTS policy for a domain.
    *   Contains `expiry` time, `include_subdomains` flag, and the `upgrade_mode` (which is always `MODE_FORCE_HTTPS` for active policies).

*   **`PKPState`:**
    *   Represents the HPKP policy for a domain.
    *   Contains `expiry` time, `include_subdomains` flag, and sets of `spki_hashes` and `bad_spki_hashes`.

*   **`Delegate`:**
    *   An interface (`TransportSecurityState::Delegate`) that allows the in-memory state to be persisted to disk. The `TransportSecurityPersister` is the concrete implementation.
    *   This is critical for ensuring that HSTS policies learned from headers survive browser restarts.

## 4. Security Considerations & Attack Surface

*   **Preload List Timeliness:** The effectiveness of the static preload list depends on the user's browser being reasonably up-to-date. The `IsBuildTimely()` method checks if the build is new enough to trust its static information. Outdated clients may have stale information.
*   **State Persistence:** If the `Delegate` fails to write the security state to disk, the browser will "forget" HSTS policies on restart, re-opening the window for SSL stripping attacks.
*   **Pinning Bypass for Local Anchors:** The `enable_pkp_bypass_for_local_trust_anchors_` flag is a crucial feature. It disables public key pinning checks if the certificate chains to a locally installed (i.e., non-public) root CA. This is necessary to allow corporate proxies, antivirus software, and other local MitM tools to function without breaking all pinned sites. Disabling this bypass would cause massive breakage in many enterprise environments.
*   **Domain Matching Logic:** The logic for finding the most specific HSTS/HPKP rule for a given hostname is security-critical. A flaw here could cause a policy to be incorrectly applied or ignored. Chromium prefers the most specific match, deviating slightly from the RFC which favors dynamic entries over static ones.

## 5. Related Files

*   `net/http/transport_security_persister.cc`: The delegate implementation that handles serializing the dynamic `TransportSecurityState` to a JSON file in the user's profile directory.
*   `net/http/transport_security_state_static.json`: The source-of-truth file containing the HSTS and HPKP preload lists. This is processed at build time to generate a compact binary representation.
*   `net/http/http_security_headers.cc`: Code responsible for parsing the `Strict-Transport-Security` and `Public-Key-Pins` headers.