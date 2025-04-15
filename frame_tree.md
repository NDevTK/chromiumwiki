# FrameTree

## 1. Component Focus
*   **Functionality:** Represents the frame structure within a `WebContents`. A `WebContents` has a primary `FrameTree` and potentially other `FrameTree`s (e.g., for prerendering, fenced frames). Each `FrameTree` has a root `FrameTreeNode` and manages the relationships between parent and child frames within that tree. It owns the `NavigationController` and `Navigator` for its frame hierarchy.
*   **Key Logic:** Managing the tree structure (adding/removing `FrameTreeNode`s), handling frame focusing within the tree (`SetFocusedFrame`), initializing the root frame (`Init`), providing iterators for tree traversal (`Nodes`, `NodesIncludingInnerTreeNodes`, etc.).
*   **Core Files:**
    *   `content/browser/renderer_host/frame_tree.h`
    *   `content/browser/renderer_host/frame_tree.cc`
    *   `content/browser/renderer_host/frame_tree_node.*`

## 2. Key Concepts & Interactions
*   **FrameTreeNode:** Represents a single frame (main frame or iframe) within the tree. See [frame_tree_node.md](frame_tree_node.md).
*   **Root Node:** The top-level node (`root_`) representing the main frame of the tree.
*   **Type:** Indicates the purpose of the tree (e.g., `kPrimary`, `kPrerender`, `kFencedFrame`).
*   **NavigationController:** Manages session history (`NavigationEntry`s) for this frame tree. Owned by `FrameTree`. See [navigation_controller.md](navigation_controller.md).
*   **Navigator:** Handles navigation logic within the frame tree. Owned by `FrameTree`. See [navigator.md](navigator.md).
*   **SiteInstance:** Each `FrameTreeNode`'s `RenderFrameHost` belongs to a `SiteInstance`. The `FrameTree::Init` function takes the initial `SiteInstance` for the root node. See [site_instance.md](site_instance.md).
*   **Delegates:** Interacts with various delegates (`WebContentsImpl`, `NavigatorDelegate`, etc.) for browser-level actions.

## 3. FrameTree Initialization (`FrameTree::Init`)

Called by `WebContentsImpl::Init` to set up a newly created `FrameTree`.

**Logic:**

1.  **Root RenderFrameHostManager Init:** Calls `root_.render_manager()->InitRoot(...)`, passing the pre-determined `main_frame_site_instance`. This likely creates the initial `RenderFrameHostImpl` for the root node in the correct `SiteInstance`.
2.  **Initial Origin State:** Calls `root_.current_frame_host()->SetOriginDependentStateOfNewFrame(...)`. This crucially sets the initial origin properties for the frame, inheriting from `opener_for_origin` if the window creation was renderer-initiated and they are in the same BrowsingInstance group. If not renderer-initiated (or different BrowsingInstance), it likely sets up a new opaque origin.
3.  **Initial NavigationEntry:** Calls `controller().CreateInitialEntry()`. This creates the first `NavigationEntry` for the frame tree, typically representing the initial empty document (`about:blank`).

**Security Relevance (Origin Confusion - VRP 40059251):**

The initialization process highlights a potential timing issue relevant to origin confusion bugs with `javascript:` or `data:` URLs:

*   The `SiteInstance` (and therefore the process) for the new window/tab is decided *before* `FrameTree::Init` is called, based primarily on the *opener*, not the target URL.
*   `FrameTree::Init` sets the *initial* document's origin based on the opener and creates an `about:blank` `NavigationEntry`.
*   The subsequent navigation to the `javascript:` or `data:` URL happens later.
*   If the security context derived from the initial `about:blank` state (influenced by the opener) differs from the context that *should* apply to the `javascript:` or `data:` URL (e.g., due to sandboxing rules applied to the opener iframe, or the nature of the scheme itself), checks performed *during* or *after* the `javascript:` execution (like `GetLastCommittedOrigin` for JS dialogs) might use the incorrect, initially inherited context, leading to origin confusion or spoofing. The VRP report specifically notes the JS dialog showing the *top-frame* origin, suggesting the initial context might incorrectly inherit from the top frame in certain cross-origin iframe scenarios.

## 4. Potential Logic Flaws & VRP Relevance
*   **Incorrect Initial Origin Inheritance:** Flaws in `SetOriginDependentStateOfNewFrame` failing to correctly determine the initial origin based on the opener, its sandbox flags, and BrowsingInstance relationship, especially for `about:blank`.
*   **Stale Context Usage:** Security checks (like for JS dialogs) using the origin associated with the initial `NavigationEntry` created by `CreateInitialEntry` instead of the context relevant to the currently executing `javascript:` URL or committed `data:` URL.
*   **Interaction with Navigation/Session History:** Issues like VRP2 6260-1 (data URLs after restore) suggest state related to the initial `FrameTree` setup might be lost or incorrectly restored, leading to process allocation errors later.

## 5. Areas Requiring Further Investigation
*   Detailed analysis of `RenderFrameHostImpl::SetOriginDependentStateOfNewFrame`.
*   How the security context (origin, SiteInstance) used by features like the `JavaScriptDialogManager` is determined, especially shortly after window creation and initial navigation.
*   The exact mechanism and timing for updating the frame's origin and `NavigationEntry` when navigating from the initial `about:blank` to `javascript:` or `data:` URLs.

*(See also: [frame_tree_node.md](frame_tree_node.md), [site_instance.md](site_instance.md), [web_contents_impl.md](web_contents_impl.md), [navigation_controller.md](navigation_controller.md), [navigator.md](navigator.md))*