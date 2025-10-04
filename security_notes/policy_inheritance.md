# Security Analysis of Policy Inheritance

## Overview

The way security policies are propagated through a frame tree is a cornerstone of web security. A child frame's capabilities are often a combination of its own declared policies and those of its ancestors. A robust and well-defined inheritance model is critical to prevent both privilege escalation and the unexpected loss of necessary permissions. This document analyzes how Chromium handles the inheritance and enforcement of key security policies, particularly the `sandbox` attribute and the `frame-ancestors` CSP directive.

## The Two Models of Policy Propagation

Chromium's implementation reveals two distinct models for how policies are applied across the frame hierarchy:

1.  **Child-Asserted Restriction (`frame-ancestors`)**: In this model, a child document asserts a policy that restricts which documents are allowed to embed it. The policy is not inherited from the parent but is checked against the parent.

2.  **Parent-Enforced Inheritance (`sandbox`)**: In this model, a parent document's policy is directly inherited by the child, and the child's own policy can only further restrict its capabilities, not relax them.

## 1. `frame-ancestors`: A Child's Defense

The `frame-ancestors` CSP directive is a prime example of the child-asserted restriction model. Its purpose is to allow a document to control its own embedding, providing a powerful defense against clickjacking attacks.

### Enforcement via `AncestorThrottle`

-   **File**: `content/browser/renderer_host/ancestor_throttle.cc`
-   **Mechanism**: The `AncestorThrottle` is a `NavigationThrottle` that runs early in the navigation process (`WillProcessResponse`).

The enforcement logic is implemented in the `EvaluateFrameAncestors` function:

-   **Traversal**: The function walks *up* the frame tree from the navigating frame's parent.
-   **Per-Ancestor Check**: For each ancestor, it invokes `CSPContext::IsAllowedByCsp`, checking the `frame-ancestors` directive from the *child's* response headers against the ancestor's origin.
-   **Immediate Blocking**: If any ancestor is not in the child's `frame-ancestors` list, the navigation is immediately blocked.

**Security Implication**: This model correctly implements the specification. It is not a policy that is "inherited" in the traditional sense; rather, it's a check that the child's asserted policy is satisfied by its ancestors. This gives documents ultimate control over their own embedding, which is a strong security posture.

## 2. `sandbox`: A Parent's Legacy

The `sandbox` attribute (and its CSP equivalent) follows the parent-enforced inheritance model. A child frame's effective sandbox policy is a combination of its own `sandbox` attribute and the sandbox policy of its parent.

### Inheritance via `FrameTreeNode`

-   **File**: `content/browser/renderer_host/frame_tree_node.cc`
-   **Mechanism**: The `FrameTreeNode` class is the central data structure for managing the frame hierarchy, and it is here that sandbox inheritance is enforced.

The core of the inheritance logic resides in the `SetPendingFramePolicy` function:

```cpp
if (parent()) {
  // Subframes should always inherit their parent's sandbox flags.
  pending_frame_policy_.sandbox_flags |=
      parent()->browsing_context_state()->active_sandbox_flags();
}
```

-   **Bitwise OR Operation**: The use of a bitwise OR (`|=`) is a key implementation detail. It combines the sandbox flags from the child's `sandbox` attribute with the *active* sandbox flags of its parent.
-   **Unidirectional Restriction**: This mechanism ensures that restrictions can only flow downwards. A child frame can add *more* restrictions to its own sandbox, but it can never remove a restriction that has been imposed by its parent. For example, if a parent has `sandbox="allow-forms"`, the child cannot re-enable script execution by specifying `sandbox="allow-scripts"`. The child's effective sandbox policy will be the union of both, meaning it will have `allow-forms` but not `allow-scripts`.

**Security Implication**: This is a critical security principle that prevents a less-trusted child frame from escaping the security context established by its parent. It ensures a "secure by default" environment where privileges must be explicitly granted and can be progressively restricted down the frame tree.

## Conclusion

Chromium's implementation of policy propagation is robust and well-designed, correctly distinguishing between the child-asserted model of `frame-ancestors` and the parent-enforced inheritance of the `sandbox`. The clear and concise implementation of these mechanisms in `AncestorThrottle` and `FrameTreeNode` respectively, demonstrates a strong commitment to security and a deep understanding of the web platform's security model. This layered and logical approach is fundamental to how Chromium protects users from a wide range of web-based threats.