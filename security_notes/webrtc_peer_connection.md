# WebRTC PeerConnection (`third_party/webrtc/pc/peer_connection.h`)

## 1. Summary

The `PeerConnection` class is the central component of the WebRTC implementation in Chromium. It is the C++ object that directly backs the `RTCPeerConnection` JavaScript API and is responsible for managing the entire lifecycle of a peer-to-peer connection. This includes orchestrating the complex state machines for signaling, ICE (Interactive Connectivity Establishment) for NAT traversal, DTLS handshakes for transport security, and the setup of all media and data channels.

Given its complexity and its role in processing large amounts of untrusted data from remote peers, the `PeerConnection` is one of the most critical attack surfaces in the browser. A vulnerability in this class or its direct dependencies could lead to remote code execution, denial of service, or the leakage of sensitive user information like internal IP addresses.

## 2. Core Concepts & Security Boundaries

The `PeerConnection` manages several distinct but interconnected security-critical state machines:

*   **JSEP (JavaScript Session Establishment Protocol):** This is the core signaling state machine.
    *   **Mechanism:** It follows an offer/answer model where peers exchange `SessionDescription` objects (containing SDP) to negotiate the parameters of the connection.
    *   **Security Boundary:** The `SetLocalDescription` and `SetRemoteDescription` methods are the primary entry points for this state machine. The SDP received from a remote peer via the signaling channel is completely untrusted and must be parsed with extreme care.

*   **ICE (Interactive Connectivity Establishment):** This is the protocol for discovering and negotiating network paths between two peers, which may be behind NATs.
    *   **Mechanism:** Each peer gathers local "candidates" (IP address and port pairs) and exchanges them via the signaling channel. The `PeerConnection` then performs connectivity checks to find a working path.
    *   **Security Boundary:** The `AddIceCandidate` method is the entry point for processing candidates from a remote peer. The primary security risk here is **IP address leakage**. The implementation must prevent a website from learning the user's private, internal IP addresses, as this would reveal sensitive network topology information.

*   **DTLS-SRTP (Datagram Transport Layer Security - Secure Real-time Transport Protocol):** This is the protocol used to secure all media and data traffic between peers.
    *   **Mechanism:** Before any application data is sent, the `PeerConnection` establishes a DTLS handshake over the negotiated ICE path. The certificates used for this handshake are self-signed and are "pinned" by fingerprinting them in the SDP. This ensures that the peer you are talking to is the same one you negotiated with via the signaling channel.
    *   **Security Boundary:** The `DtlsTransport` and `SrtpTransport` classes, managed by the `PeerConnection`, are responsible for the entire DTLS handshake and encryption process. A vulnerability in the underlying TLS implementation (BoringSSL) or in how these classes manage the handshake state could lead to a compromise of the connection's confidentiality and integrity.

## 3. Security-Critical Logic & Vulnerabilities

*   **SDP Parsing (`SdpOfferAnswerHandler`):**
    *   **Risk:** The SDP blob received from a remote peer is complex, text-based, and completely untrusted. It is a massive parsing attack surface. A classic buffer overflow, integer overflow, or type confusion bug in the SDP parser could be triggered by a malicious peer, leading to remote code execution in the context of the renderer process (or whichever process is hosting the WebRTC stack).
    *   **Mitigation:** All SDP parsing logic must be written with the assumption that the input is malicious. It must rigorously validate all lengths, offsets, and values.

*   **IP Address Leakage in ICE Candidates:**
    *   **Risk:** An attacker's website could use WebRTC to discover a user's internal IP address, which is a significant privacy leak.
    *   **Mitigation:** The WebRTC stack has complex logic to prevent this. By default, it uses mDNS to generate `.local` hostnames to mask private IP addresses in ICE candidates. It will only expose raw IP addresses under specific circumstances (e.g., if the user grants permission). A bug in this masking or permission-checking logic would be a critical privacy vulnerability. The `PortAllocator` is the class responsible for gathering candidates and applying this policy.

*   **DTLS Implementation Vulnerabilities:**
    *   **Risk:** The security of the entire connection depends on the correctness of the DTLS implementation (BoringSSL). A vulnerability like Heartbleed in the TLS library would allow an attacker to read arbitrary memory from the peer.
    *   **Mitigation:** This relies on the security of the underlying crypto library and ensuring that the `PeerConnection` and `DtlsTransport` classes use it correctly, for example, by properly validating the remote peer's certificate fingerprint against the one provided in the SDP.

*   **State Machine Confusion:**
    *   **Risk:** A bug in the JSEP state machine that allows an invalid state transition (e.g., processing an SDP answer when an offer is pending) could lead to a confused internal state, potentially bypassing security checks or causing a crash.
    *   **Mitigation:** The `SetLocalDescription` and `SetRemoteDescription` methods must be extremely careful to validate the current `signaling_state()` before processing a new description.

## 4. Key Functions

*   `SetLocalDescription(...)` / `SetRemoteDescription(...)`: The most critical entry points for processing untrusted SDP and driving the main state machine.
*   `AddIceCandidate(...)`: The entry point for processing untrusted network candidates.
*   `GetConfiguration()` / `SetConfiguration(...)`: Manages the configuration of the connection, including which ICE servers to use. A malicious website trying to force the use of a malicious TURN server could be a potential attack vector.
*   `Close()`: Responsible for securely tearing down all transports, channels, and associated state.

## 5. Related Files

*   `pc/sdp_offer_answer.h`: Contains the core logic for parsing and generating SDP. A primary source of attack surface.
*   `pc/jsep_transport_controller.h`: Manages the ICE and DTLS transports.
*   `p2p/base/port_allocator.h`: Responsible for gathering local ICE candidates and implementing the IP address privacy mitigations.
*   `api/jsep.h`: Defines the `SessionDescriptionInterface` and `IceCandidateInterface`, which are the primary data structures exchanged over the signaling channel.
*   `rtc_base/rtc_certificate.h`: Manages the self-signed certificates used for the DTLS handshake.