# Security Analysis of `content/browser/per_web_ui_browser_interface_broker.cc`

## Summary

The `PerWebUIBrowserInterfaceBroker` is a security-critical component responsible for managing Mojo interface requests from highly privileged WebUI pages (e.g., `chrome://settings`, `chrome://history`). WebUI pages are a core part of the browser's user interface and have access to powerful, browser-internal APIs. This broker acts as the primary gatekeeper, ensuring that a given WebUI page can only access the specific set of interfaces it has been explicitly granted permission to use.

## The Core Security Principle: Whitelist and Terminate

The security model of the `PerWebUIBrowserInterfaceBroker` is defined by two key principles: an explicit whitelist of interfaces and a "fail-deadly" response to any violation.

1.  **Explicit Whitelist (`binder_map_`)**: At its creation, the broker is populated with a `binder_map_`. This map contains an explicit list of all the Mojo interfaces that the specific WebUI page is allowed to request. This is a strict whitelist; any interface not in this map is considered forbidden. This adheres to the security principle of "default-deny."

2.  **Fail-Deadly Violation Handling**: The `GetInterface` method is the heart of the enforcement mechanism. When a WebUI page requests an interface, this method looks for a corresponding entry in the `binder_map_`. If no entry is found, the broker doesn't just reject the request—it assumes the renderer is compromised or severely broken and immediately terminates it.

```cpp
// from PerWebUIBrowserInterfaceBroker::GetInterface
if (!binder_map_.TryBind(&*controller_, &receiver)) {
  LOG(ERROR) << "Per WebUI interface binder missing for: " << name;
  // WebUI page requested an interface that's not registered
  ShutdownWebUIRenderer(*controller_);
}
```

This call to `ShutdownWebUIRenderer` is the most severe and secure response possible. It kills the renderer process, preventing a potentially malicious page from making any further attempts to access unauthorized APIs. This is a critical defense against exploits that might seek to escape the confines of a WebUI page.

## Granular Privileges: A Broker per WebUI

A key aspect of this architecture is its granularity. A new `PerWebUIBrowserInterfaceBroker` is created for each individual WebUI controller. This means that `chrome://settings` gets its own broker with its own set of allowed interfaces, and `chrome://downloads` gets a *different* broker with a *different* set of interfaces.

This is a powerful application of the **Principle of Least Privilege**. It ensures that a compromise in the `chrome://history` page, for example, cannot be used to access the powerful, security-sensitive APIs exposed only to the `chrome://settings` page. This fine-grained separation of privileges is essential for containing the potential damage of a vulnerability in any single WebUI page.

## Conclusion

The `PerWebUIBrowserInterfaceBroker` is a vital security boundary that protects the browser from its own highly privileged UI components. Its strength lies in its strict, whitelist-based approach and its unforgiving "fail-deadly" policy of terminating any renderer that requests an unauthorized interface. This design ensures that WebUI pages, despite their power, are kept on a tight leash, with access only to the specific APIs they need to function, and nothing more.