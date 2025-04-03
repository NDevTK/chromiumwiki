# Plugin Process Security in Chromium

This page documents potential security vulnerabilities within plugin processes in Chromium, focusing on the handling of URL requests by Pepper plugins in `ppapi/proxy/url_request_info_resource.cc`.

## Potential Vulnerabilities:

*   **Code Injection:** Vulnerabilities in plugin processes could allow code injection.  Malicious plugins could potentially exploit vulnerabilities in URL request handling to inject code into the renderer process.
*   **Memory Corruption:** Memory corruption vulnerabilities could be exploited.
*   **Sandboxing Issues:** Sandbox escapes are a concern.
*   **Resource Exhaustion:** Plugin processes could cause DoS.
*   **Insufficient Input Validation:**  Insufficient input validation of URL request parameters in `url_request_info_resource.cc` could allow a malicious plugin to bypass security checks or perform injection attacks if the renderer's validation is insufficient.
*   **File System Access:**  The `AppendFileToBody` function in `url_request_info_resource.cc` could allow unauthorized file access if not handled securely in the renderer.
*   **Information Leaks:** `getThumbnail()` CHECK leaks number of available PDF pages (Fixed, Commit: 40059101).

## Further Analysis and Potential Issues:

*   **Sandboxing:** Thoroughly review sandboxing mechanisms.
*   **Memory Management:** Implement robust memory management.
*   **Resource Limits:** Implement resource limits.
*   **Input Validation:** Implement robust input validation.
*   **URL Request Handling:**  The `URLRequestInfoResource` class in `url_request_info_resource.cc` handles URL requests from Pepper plugins.  This class relies heavily on the renderer process for security validation.  Key functions to analyze include `SetProperty`, `AppendDataToBody`, `AppendFileToBody`, `SetUndefinedProperty`, `SetBooleanProperty`, `SetIntegerProperty`, and `SetStringProperty`.  Potential vulnerabilities include insufficient input validation, data handling and sanitization issues, file system access vulnerabilities, and insecure error handling.  The renderer's validation logic is a critical security boundary.

## Areas Requiring Further Investigation:

*   Implement robust sandboxing.
*   Implement robust memory management.
*   Implement resource limits.
*   Implement robust input validation.
*   **Renderer Validation:**  The renderer process's validation of URL request data from plugins is a critical security boundary and needs thorough review.  Any weaknesses in this validation could be exploited by malicious plugins.
*   **Data Sanitization:**  The sanitization of URL request data, including URLs, headers, and other parameters, needs to be carefully reviewed to prevent injection attacks.
*   **File Access Control:**  The renderer's handling of file access requests from plugins, especially through the `AppendFileToBody` function, requires further analysis to ensure proper sandboxing and access control.

## Files Reviewed:

*   `ppapi/proxy/url_request_info_resource.cc`

## Key Functions and Classes Reviewed:

*   `URLRequestInfoResource::SetProperty`, `URLRequestInfoResource::AppendDataToBody`, `URLRequestInfoResource::AppendFileToBody`, `URLRequestInfoResource::SetUndefinedProperty`, `URLRequestInfoResource::SetBooleanProperty`, `URLRequestInfoResource::SetIntegerProperty`, `URLRequestInfoResource::SetStringProperty`
