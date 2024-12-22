# NavigationRequest Core

This page details the core concepts of the `NavigationRequest` class and its related files.

## Core Concepts

The `NavigationRequest` class manages the navigation lifecycle and determines the `UrlInfo` for the navigation. Logic errors here can result in incorrect isolation decisions for navigations, potentially leading to security breaches.

### Key Areas of Concern

-   Incorrectly determining the `UrlInfo` for the navigation.
-   Errors in handling redirects and cross-origin navigations.
-   Incorrectly applying security policies during navigation.
-   Potential issues with the interaction between navigation and other security mechanisms.

### Related Files

-   `content/browser/renderer_host/navigation_request.cc`
-   `content/browser/renderer_host/navigation_request.h`

### Files Analyzed:
* `content/browser/renderer_host/navigation_request.cc`
