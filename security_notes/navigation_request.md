# Security Analysis of navigation_request.cc

## Overview

`navigation_request.cc` is a critical file in the Chromium browser architecture, responsible for managing the lifecycle of a navigation from initiation to commit. It acts as a central authority for enforcing a wide range of security policies, making it a key component in the browser's security model. This document provides a detailed analysis of the security-critical aspects of `navigation_request.cc`.

## Key Security Responsibilities

### 1. Navigation Initiation and Parameter Sanitization

The `NavigationRequest` class is responsible for creating and managing navigation requests, which can be initiated by either the browser or a renderer process. A fundamental security principle is to treat all data from the renderer as untrusted.

- **Differentiated Initiation**: The distinction between `NavigationRequest::CreateBrowserInitiated` and `NavigationRequest::CreateRendererInitiated` is crucial. Renderer-initiated navigations are subject to more stringent security checks because the renderer process is considered a less trusted environment.

- **Parameter Sanitization**: The `NavigationRequest` constructor and its associated methods perform rigorous sanitization of navigation parameters to prevent security vulnerabilities:
    - **Referrer Policy**: The `Referrer::SanitizeForRequest` function is consistently used to enforce the referrer policy, preventing the leakage of sensitive information across origins.
    - **URL Sanitization**: The code includes checks to block navigations to invalid or disallowed URLs, such as `javascript:` URLs in contexts where they are not permitted.
    - **Header Sanitization**: The `AddAdditionalRequestHeaders` function adds or overwrites critical security headers. It ensures that headers like `Upgrade-Insecure-Requests` are present and that renderer-provided headers such as `Origin` and `User-Agent` are either sanitized or replaced with trustworthy values.

### 2. Security Policy Enforcement

`navigation_request.cc` is a central point for the enforcement of numerous web security policies:

- **Content Security Policy (CSP)**: `CheckContentSecurityPolicy` is invoked at multiple stages of the navigation (before the request, on redirects, and on response) to enforce CSP directives like `form-action` and `frame-src`. This is critical for mitigating cross-site scripting (XSS) and other injection attacks.

- **Cross-Origin Opener Policy (COOP)**: The `coop_status_` member is used to manage and enforce COOP. The `OnRequestRedirected` and `OnResponseStarted` methods call `coop_status_.SanitizeResponse` and `coop_status_.EnforceCOOP` to ensure that navigations respect the COOP of the originating context and the response, preventing cross-window attacks.

- **Cross-Origin Embedder Policy (COEP)**: The `EnforceCOEP` and `CheckResponseAdherenceToCoep` methods are responsible for enforcing COEP. This policy is essential for enabling cross-origin isolation and protecting against speculative execution attacks like Spectre.

- **Sandbox Flags**: The `NavigationRequest` computes and enforces sandbox flags that restrict the capabilities of the document to be loaded. This is particularly important for features like fenced frames, which have a set of forced sandbox flags to enhance security.

- **Private Network Access (PNA)**: The `UpdatePrivateNetworkRequestPolicy` and `DerivePrivateNetworkRequestPolicy` functions enforce Private Network Access checks, which restrict the ability of websites to make requests to private networks, mitigating CSRF-like attacks against internal network devices.

### 3. Process Model and Site Isolation

The `NavigationRequest` plays a vital role in maintaining the integrity of the browser's process model and site isolation architecture:

- **RenderFrameHost Selection**: The `RenderFrameHostManager::GetFrameHostForNavigation` method is called to select an appropriate `RenderFrameHost` for the navigation. This is a critical security decision that determines the renderer process responsible for the navigation. The selection is based on the URL, the initiator, and security policies such as site isolation and origin-agent-cluster.

- **Origin-Agent-Cluster**: The `NavigationRequest` handles the `Origin-Agent-Cluster` header, which allows sites to opt into origin-keyed agent clusters. This affects how documents are grouped in processes and has both performance and security implications. The `DetermineOriginAgentClusterEndResult` method and related functions are responsible for managing this.

- **SiteInstance Management**: The `NavigationRequest` interacts with `SiteInstance` objects to ensure that documents are loaded in the correct process, thereby respecting and enforcing site isolation boundaries.

### 4. Handling of Special URL Schemes

`navigation_request.cc` includes specialized logic for handling various URL schemes to prevent them from being abused:

- **`about:blank` and `about:srcdoc`**: These schemes are handled with care to ensure they inherit the correct origin and security policies from their initiator or parent frame.
- **`data:` URLs**: Navigations to `data:` URLs are managed to ensure they commit in the correct `SiteInstance`, preventing potential spoofing or cross-origin attacks.
- **Blob URLs**: Blob URLs are resolved through the `BlobURLRegistry`, which restricts access to contexts with the appropriate permissions.

### 5. Back-Forward Cache and Prerendering

The `NavigationRequest` is also involved in the secure management of the back-forward cache and prerendering:

- **Back-Forward Cache**: The code interacts with the `BackForwardCache` to restore pages. This process includes security checks to ensure that the restored page is still safe to display (e.g., verifying that the user agent override has not changed).

- **Prerendering**: `navigation_request.cc` manages the activation of prerendered pages. This is a complex operation that involves running security checks, such as `CommitDeferringConditions`, before a prerendered page is activated.

### 6. Fenced Frames

Fenced frames introduce new security challenges, and `navigation_request.cc` is responsible for enforcing their security model:

- **URN Mapping**: For fenced frames, the `NavigationRequest` handles the mapping of URNs to URLs via the `FencedFrameURLMapping`.
- **Security Restrictions**: It enforces numerous security restrictions on fenced frames, including forced sandbox flags and the requirement for the `Supports-Loading-Mode: fenced-frame` HTTP header.

## Potential Attack Vectors and Mitigations

- **Navigation Parameter Spoofing**:
  - **Threat**: A compromised renderer could attempt to spoof navigation parameters to bypass security checks.
  - **Mitigation**: The browser process treats all data from the renderer as untrusted. `NavigationRequest` is a primary line of defense, performing rigorous sanitization of all incoming parameters.

- **Information Leaks**:
  - **Threat**: An attacker could try to leak information across origins through mechanisms like the referrer header or by embedding a cross-origin document.
  - **Mitigation**: The enforcement of Referrer Policy, COEP, and CORP by `NavigationRequest` is the primary mitigation against these threats.

- **Process Model Abuse**:
  - **Threat**: An attacker could attempt to trick the browser into loading a document in the wrong process, thereby breaking site isolation.
  - **Mitigation**: The logic within `RenderFrameHostManager` and `NavigationRequest` for selecting a `RenderFrameHost` is critical for preventing this type of attack.

- **Special URL Scheme Abuse**:
  - **Threat**: An attacker could abuse special URL schemes like `about:blank` or `data:` to bypass security policies.
  - **Mitigation**: The careful and specialized handling of these schemes within `NavigationRequest` is essential to prevent their misuse.

- **Back-Forward Cache/Prerendering Attacks**:
  - **Threat**: An attacker might try to exploit the back-forward cache or prerendering mechanisms to bypass security checks.
  - **Mitigation**: The security checks performed during the activation of cached or prerendered pages are designed to prevent such attacks.

## Conclusion

`navigation_request.cc` is a cornerstone of Chromium's security architecture. Its comprehensive approach to parameter sanitization, security policy enforcement, and process model management makes it a formidable defense against a wide range of web-based attacks. A deep understanding of its functionality is essential for anyone working on browser security.