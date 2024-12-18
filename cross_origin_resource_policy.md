# Cross-Origin Resource Policy (CORP)

This page documents potential security vulnerabilities related to Cross-Origin Resource Policy (CORP) in Chromium. CORP is a security mechanism that allows web servers to control how their resources are accessed and shared across origins.

## Potential Vulnerabilities:

* **Policy Enforcement:** Incorrect or incomplete enforcement of CORP could allow unauthorized cross-origin access to resources.  The `IsBlocked` function in `cross_origin_resource_policy.cc` is crucial for CORP enforcement and needs thorough review.  Analyze the handling of different CORP values (`same-origin`, `same-site`, `cross-origin`) and their interaction with various request modes and credentials.
* **Bypass Techniques:** Attackers might find ways to bypass CORP, either through flaws in the implementation or by exploiting interactions with other browser features or security headers. Regular testing and analysis are essential.  Consider scenarios involving redirects, iframes, or service workers.
* **Compatibility Issues:** CORP might not be compatible with all web pages or features, potentially leading to unexpected behavior or broken functionality. Thorough testing is needed to ensure compatibility across different browsers and web platforms.  Test with various web technologies and frameworks, including older browsers and different document modes.

## Further Analysis and Potential Issues:

* **`services/network/public/cpp/cross_origin_resource_policy.cc`:** This file contains the core CORP implementation. The `IsBlocked` function determines if a request should be blocked based on the CORP header and other factors.  This function's logic, including its interaction with COEP and other security contexts, needs careful review.
* **`third_party/blink/renderer/core/loader/mixed_content_checker.h`:** The `MixedContentChecker` class handles mixed content checks, which can interact with CORP.  Analyze how mixed content checks are performed and how they might affect CORP enforcement.
* **`third_party/blink/renderer/core/loader/subresource_filter.cc` and `.h`:** The `SubresourceFilter` class filters subresource loads, which can be influenced by CORP.  Review the `AllowLoad` function and other relevant parts of the code for potential vulnerabilities related to CORP bypasses or incorrect enforcement.
* **`third_party/blink/renderer/core/loader/worker_fetch_context.cc`:** The `WorkerFetchContext` class handles fetch requests in worker contexts, which can be subject to CORP.  Analyze how CORP is enforced in worker contexts and whether there are any potential bypasses.
* **Fetch Specification Integration:** The Fetch specification details the CORP header and its interaction with the fetch algorithm.  Careful review of the specification and Chromium's implementation is crucial.  Pay attention to how CORP interacts with other security mechanisms defined in the Fetch specification, such as COEP and CORS.

## Areas Requiring Further Investigation:

* **CORP and Caching:**  The interaction between CORP and browser caching mechanisms needs further analysis to ensure that cached resources are handled correctly and securely according to CORP policies.
* **CORP and Service Workers:**  The interaction between CORP and service workers should be investigated, as service workers can intercept and modify network requests, potentially bypassing CORP restrictions.
* **CORP and Redirects:**  The handling of redirects in the context of CORP needs further analysis to ensure that CORP policies are correctly enforced throughout the redirect chain.
* **Error Handling and Reporting:**  The error handling and reporting mechanisms related to CORP should be reviewed to ensure that errors are handled gracefully and that violations are reported correctly.
* **Interaction with Other Security Headers:**  The interaction between CORP and other security headers, such as CSP and COOP, should be analyzed for potential conflicts or bypasses.

## Files Reviewed:

* `services/network/public/cpp/cross_origin_resource_policy.cc`
* `third_party/blink/renderer/core/loader/mixed_content_checker.h`
* `third_party/blink/renderer/core/loader/subresource_filter.cc`
* `third_party/blink/renderer/core/loader/subresource_filter.h`
* `third_party/blink/renderer/core/loader/worker_fetch_context.cc`
