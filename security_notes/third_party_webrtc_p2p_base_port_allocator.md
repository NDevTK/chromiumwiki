# Security Analysis of `port_allocator.cc`

This document provides a security analysis of the `port_allocator.cc` file from the WebRTC codebase. The `PortAllocator` class acts as a factory for `PortAllocatorSession` objects, which are responsible for creating the various types of ports (UDP, TCP, STUN, TURN) used in an ICE session. This file is central to enforcing high-level security and privacy policies for ICE.

## 1. Candidate Sanitization and Privacy

The most critical security function within this file is `PortAllocator::SanitizeCandidate` (line 319). This method is responsible for stripping sensitive information from ICE candidates before they are signaled to a remote peer, which is the primary defense against IP address leakage.

- **mDNS Obfuscation:** The function checks if mDNS obfuscation is enabled. If it is, it sets a flag (`use_hostname_address`) to replace the local IP address in host candidates with a `.local` mDNS hostname. This is a powerful privacy-preserving feature that prevents the actual local IP from being exposed.

- **Related Address Filtering:** The function contains crucial logic to filter the `related_address` from STUN and TURN candidates. The `related_address` typically contains the local IP address of the client. Exposing it would leak information about the user's local network topology. The decision to filter is based on several conditions:
  - **Disabling Host Candidates:** If host candidates (`CF_HOST`) are disabled via the candidate filter, the allocator will also strip the related address from STUN candidates. This provides a consistent policy: if the application doesn't want to expose host IPs, it shouldn't leak them via STUN candidates either.
  - **Disabling Adapter Enumeration:** Flags like `PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION` also trigger the filtering of related addresses, reinforcing the goal of hiding local network details.
  - **mDNS Obfuscation:** When mDNS obfuscation is active, related addresses are also filtered to ensure the local IP is not leaked through a different channel.

## 2. Candidate Generation Control

The `PortAllocator` provides a mechanism to control which types of ICE candidates are created, which is a fundamental tool for enforcing security policies.

- **`SetCandidateFilter` (line 291):** This function sets the `candidate_filter_`, which can be configured to allow or disallow host, server-reflexive (STUN), and relay (TURN) candidates.
- **Security Implication:** An application can use this filter to enforce specific data paths. For example, by setting the filter to `CF_RELAY`, an application can ensure that all media traffic is forced through a TURN server. This is a common requirement in enterprise or high-security environments where direct P2P connections are disallowed for policy reasons.

## 3. Secure Configuration

The `PortAllocator` is the central point for configuring STUN and TURN servers.

- **`SetConfiguration` (line 152):** This function takes a list of `RelayServerConfig` objects.
- **Secure TURN:** The `RelayServerConfig` constructors (lines 35-65) include logic to handle secure protocols. Specifically, if a relay is configured with `PROTO_TCP` and a `secure` flag is set to true, the protocol is automatically upgraded to `PROTO_TLS`. This encourages and simplifies the use of encrypted TURNS, which is critical for protecting the integrity and confidentiality of TURN allocations and traffic.

## 4. Session Pooling and Credential Management

- **Candidate Pooling:** The allocator can pre-allocate and pool `PortAllocatorSession` objects to speed up future ICE negotiations.
- **`restrict_ice_credentials_change_`:** This flag (set via `set_restrict_ice_credentials_change`, line 133) provides a security safeguard for the pooling mechanism. When true, a pooled session can only be reused if the new ICE credentials match the ones it was created with. This helps prevent logical errors where a session might be inadvertently reused across different security contexts.

## Summary of Potential Security Concerns

1.  **Complexity of Sanitization Logic:** The logic in `SanitizeCandidate` for deciding when to filter related addresses is complex, depending on multiple flags (`candidate_filter_`, `PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION`, mDNS status). A misconfiguration or a bug in this logic could lead to the unintentional leakage of private IP addresses, undermining user privacy.
2.  **Reliance on Correct Configuration:** The security of the ICE process heavily relies on the application correctly configuring the `PortAllocator`. An application that fails to set an appropriate candidate filter or provides insecure TURN server details (e.g., TURN over UDP/TCP instead of TLS) could expose users to risk.
3.  **Information Leakage via New Candidate Types:** The sanitization logic is written to handle known candidate types (host, srflx, prflx, relay). If a new candidate type were introduced in the future, the `SanitizeCandidate` function would need to be updated to handle it correctly, otherwise it could become a new vector for information leakage.