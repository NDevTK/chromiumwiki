# Security Architecture of Process and Frame Hosts

This document provides an overview of the security architecture of `RenderProcessHost` and `RenderFrameHost`, which are critical components in Chromium's security model.

## RenderProcessHost

`RenderProcessHost` represents the browser-side object for a single renderer process. It is responsible for managing the lifecycle of the renderer process and enforcing security policies.

### Process Management and Lifecycle

- **Process Allocation**: `RenderProcessHost` is responsible for creating and managing renderer processes. It implements a process model that aims to isolate sites from each other.
- **Process Reuse**: To conserve resources, Chromium reuses renderer processes for navigations to the same site. `RenderProcessHost` implements policies for process reuse, including the management of a spare `RenderProcessHost` to speed up navigations.
- **Shutdown**: It handles the shutdown of renderer processes, including a "fast shutdown" mechanism that can terminate a process quickly if it's not running critical tasks like unload handlers.

### Security Mechanisms

- **Sandboxing**: `RenderProcessHost` is a key component in the sandboxing architecture. It launches renderer processes with a restricted set of permissions, which helps to contain the impact of a compromise in the renderer.
- **Process Lock**: It enforces a "process lock" that restricts a renderer process to a specific site. This is a fundamental aspect of site isolation, preventing a compromised renderer from accessing data from other sites.
- **Keep-Alive**: `RenderProcessHost` has a keep-alive mechanism that prevents a renderer process from being shut down prematurely. This is important for security because it ensures that processes with active frames are not terminated, which could lead to denial-of-service vulnerabilities.

## RenderFrameHost

`RenderFrameHost` represents a single frame in a web page's frame tree. It is responsible for managing the lifecycle of the frame and enforcing security policies at the frame level.

### Navigation Handling

- **URL Validation**: `RenderFrameHost` validates all navigation requests to ensure that the renderer process is allowed to navigate to the requested URL. This is a critical defense against a compromised renderer trying to navigate to a malicious site.
- **Origin Checks**: It performs origin checks to enforce the same-origin policy. This prevents scripts in one frame from accessing data in another frame if they have different origins.
- **IPC Handling**: It handles frame-specific IPC messages from the renderer. This includes messages related to navigation, user input, and other browser features.

### Security-Critical Aspects

- **Content Security Policy (CSP)**: `RenderFrameHost` enforces CSP, which is a mechanism for preventing cross-site scripting (XSS) attacks.
- **Permissions Policy**: It enforces Permissions Policy, which allows a site to control which features and APIs can be used in a frame.
- **Back-Forward Cache**: `RenderFrameHost` has logic for handling the back-forward cache, which has security implications. For example, it ensures that sensitive information is not leaked when a page is restored from the cache.
- **Fenced Frames**: It has extensive logic for handling fenced frames, which are designed to be isolated from the embedding page. This includes enforcing restrictions on communication between the fenced frame and the embedder.