# Omnibox Security Analysis

**Component Focus:** Chromium's omnibox (address bar) functionality, including input parsing (`components/omnibox/browser/autocomplete_input.cc`), URL formatting and fixing (`components/url_formatter/`), and UI rendering (`chrome/browser/ui/views/omnibox/`, `chrome/browser/ui/views/location_bar/`). Key classes include `AutocompleteInput`, `OmniboxViewViews`, and `LocationBarView`.

**Potential Logic Flaws & VRP Relevance:**

*   **URL Parsing & Canonicalization Issues:** Ambiguities or errors in `AutocompleteInput::Parse`, `url_formatter::SegmentURL`, or `url_formatter::FixupURL` could lead to misinterpretation of user input, potentially enabling URL spoofing or bypassing security checks. This is especially relevant for inputs with unusual characters, mixed schemes, or non-standard TLDs.
    *   **VRP Pattern (High - $8,500):** Security: Android: URL spoofing in address bar if scheme is later in URL (Fixed, Commit: `40072988`). Vulnerability occurs when the scheme is placed later in the URL, potentially bypassing security checks in `AutocompleteInput::Parse`. See VRP #28 (VRP.txt L24-28), VRP #1652 (VRP2.txt L1652-1687).
    *   **VRP Pattern (High - $7,000):** Security: Android address bar hidden after slow navigation finishes, if slow nav is initiated on page load (Fixed, Commit: `379652406`). Although component is Navigation, this directly affects Omnibox visibility. See VRP #41 (VRP.txt L38-42), VRP #2851 (VRP2.txt L2851-2916).
    *   **VRP Pattern (High - $6,000):** Security: Android address bar URL spoof if page is scrolling and tab is switched (Fixed, Commit: `343938078`). Race condition leading to persistent incorrect URL. See VRP #47 (VRP.txt L45-49), VRP #3274 (VRP2.txt L3274-3391).
    *   **VRP Pattern (Medium - $2,000):** Security: Origin spoof in external protocol dialogs via server-side redirect to external protocol (Fixed, Commit: `40055515`). Dialog shows initiator origin, not redirector. See VRP #254 (VRP.txt L254-259), VRP #9087 (VRP2.txt L9087-9112).
    *   **VRP Pattern (Medium - $1,000):** Security: Web Share dialog URL is incorrectly elided in Android (Fixed, Commit: `40061104`). Affects URL display originating from Omnibox context. See VRP #290 (VRP.txt L290-294).
    *   **VRP Pattern:** URL Spoof after crash (Fixed, Commit: `40057561`). Related to maintaining correct URL state after renderer crash. See VRP #318 (VRP.txt L318-322), VRP #9503 (VRP2.txt L9503-9510).
    *   **VRP Pattern:** URL Spoofing via Bidirectional Domain Names / RTL characters. See VRP #5139 (VRP2.txt L5139-5154), VRP #1618 (VRP2.txt L1618-1630).
    *   **VRP Pattern:** URL Spoofing via very long URLs. See VRP #5977 (VRP2.txt L5977-5984), VRP #1856 (VRP2.txt L1856-1864).
*   **Hostname/IP Address Validation Bypass:** Weaknesses in hostname validation (`net::IsCanonicalizedHostCompliant`) or IP address parsing (`net::CanonicalizeHost`, including handling of different component counts) could allow spoofing or navigation to unintended destinations. Inputs designed to confuse these checks (e.g., looking like IPs but aren't, using invalid characters) are suspect.
*   **Scheme Handling & Spoofing:** Incorrect identification or handling of schemes (especially `view-source:`, `javascript:`, `file:`, `filesystem:`, or custom/unknown schemes) within `AutocompleteInput::Parse` could lead to unexpected behavior or UI spoofing.
*   **Input Validation (General):** Beyond URL structure, insufficient validation in UI elements (`OmniboxViewViews`, `LocationBarView`) for input methods (IME, paste, drag-and-drop) could lead to injection or state confusion. Functions like `SetImePrefixAutocompletion`, `OnDrop`, `ShowContextMenu` need scrutiny.
*   **Suggestion/Autocomplete Manipulation:** Flaws in how suggestions or autocomplete entries are generated, handled, or displayed could leak sensitive data or be manipulated to facilitate phishing/spoofing.
*   **UI State Confusion/Spoofing:** Logic errors in `LocationBarView` related to updating the security chip (`GetSecurityChipColor`, `SecurityStateTabHelper`), page actions, or general UI state (`OnFocus`, `OnBlur`, `RefreshContentSettingViews`) could mislead the user about the security context or origin. Interactions with overlays (PiP, dialogs, keyboard) are relevant here.
    *   **VRP Pattern:** Various VRPs show PiP, FedCM prompts, extension popups obscuring UI elements, potentially including Omnibox-related prompts or requiring interaction while Omnibox is hidden/spoofed.

## Further Analysis and Potential Issues:

*   **Codebase Research:** The core logic for input classification resides in `AutocompleteInput::Parse`. Its interaction with `url_formatter::FixupURL` and `url_formatter::SegmentURL` is critical. The handling of various URL components (scheme, host, username, path), IP address formats, and TLDs presents a large attack surface. UI logic in `LocationBarView` translates the parsed state into visual indicators. VRP reports highlight sensitivity in Android URL parsing, race conditions during navigation/scrolling/tab switching, and interactions with other UI elements like PiP.
*   **Security Level Determination:** The security level shown in the location bar depends on `SecurityStateTabHelper` (`components/security_state/core/security_state.cc`). Vulnerabilities could arise if this component receives incorrect information from the navigation or parsing process, or if its own logic is flawed (e.g., handling certificate errors, mixed content, specific schemes).
*   **Extension & Feature Interactions:** How does omnibox input/parsing interact with extensions (e.g., keyword searches, default search provider overrides) or features like `view-source:`, `blob:`, `filesystem:`? Can these interactions bypass standard parsing logic?
*   **Edge Cases:** Focus on edge cases like extremely long inputs, inputs with mixed scripts or control characters, RTL/Bidi characters, inputs resembling IP addresses or file paths but intended as queries, and inputs involving nested schemes (`view-source:`, `filesystem:`). Android-specific timing and UI lifecycle edge cases appear particularly relevant based on VRP data.

## Code Analysis:

Key function: `AutocompleteInput::Parse` in `components/omnibox/browser/autocomplete_input.cc` determines input type.

```c++
// Simplified logic highlights:
metrics::OmniboxInputType AutocompleteInput::Parse(
    const std::u16string& text,
    const std::string& desired_tld,
    const AutocompleteSchemeClassifier& scheme_classifier,
    url::Parsed* parts,
    std::u16string* scheme,
    GURL* canonicalized_url) {

  // ... initial checks ...

  // Use URLFixerUpper to handle non-scheme ":" ambiguity.
  const std::u16string parsed_scheme(url_formatter::SegmentURL(text, parts));
  // ...

  // Canonicalize input using FixupURL. Bail if invalid.
  *canonicalized_url = url_formatter::FixupURL(base::UTF16ToUTF8(text), desired_tld);
  if (!canonicalized_url->is_valid())
    return metrics::OmniboxInputType::QUERY;

  // ... Handle file:, javascript:, other explicit non-http(s) schemes ...
  // If scheme is unknown, try prefixing with http:// to check for user:pass@host format.

  // *** Heuristics for HTTP/HTTPS or no scheme ***

  // Analyze canonicalized host
  url::CanonHostInfo host_info;
  net::CanonicalizeHost(canonicalized_url->host(), &host_info);
  const size_t registry_length = net::registry_controlled_domains::GetCanonicalHostRegistryLength(...);
  const bool has_known_tld = registry_length != 0;

  // Check for non-compliant hostnames / .invalid TLD
  if (host_info.family == url::CanonHostInfo::NEUTRAL &&
      (!net::IsCanonicalizedHostCompliant(canonicalized_url->host()) || ...)) {
    // If space in original host -> QUERY
    // Else if explicit scheme or known TLD -> UNKNOWN (allows infobar)
    // Else -> QUERY
  }

  // Check IP addresses (V6 -> URL; V4 needs more checks)
  if (host_info.family == url::CanonHostInfo::IPV4) {
    // Re-canonicalize *original* host input to check components typed by user
    net::CanonicalizeHost(base::UTF16ToUTF8(original_host), &host_info);
    if (host_info.num_ipv4_components == 4) return metrics::OmniboxInputType::URL;
    // ... handle 0.0.0.0 specifically ...
    // If first octet is 0 (and not 0.0.0.0) -> QUERY
  }

  // Explicit http(s) scheme -> URL
  if (parts->scheme.is_nonempty()) return metrics::OmniboxInputType::URL;

  // Trailing slash (if username has no space) -> URL
  if (parts->path.is_nonempty() && !username_has_space) { ... }

  // IPV4 with 2/3 components (now that scheme/slash didn't apply) -> QUERY
  if ((host_info.family == url::CanonHostInfo::IPV4) && (host_info.num_ipv4_components > 1)) return metrics::OmniboxInputType::QUERY;

  // Username with space -> UNKNOWN
  if (username_has_space) return metrics::OmniboxInputType::UNKNOWN;

  // >1 non-host component -> URL
  if (NumNonHostComponents(*parts) > 1) return metrics::OmniboxInputType::URL;

  // Username without desired_tld -> UNKNOWN (likely email)
  if (canonicalized_url->has_username() && desired_tld.empty()) return metrics::OmniboxInputType::UNKNOWN;

  // Known TLD / localhost / port -> URL
  if (has_known_tld || canonicalized_url->DomainIs("localhost") || canonicalized_url->has_port()) return metrics::OmniboxInputType::URL;
  // ... special TLDs (.example, .test, .local) ...

  // Default fallback -> UNKNOWN
  return metrics::OmniboxInputType::UNKNOWN;
}
```

**Areas Requiring Further Investigation:**

*   **Android-Specific Parsing/Validation:** Investigate potential bypasses for the checks implemented in `AutocompleteInput::Parse` and `url_formatter::FixupURL` to prevent URL spoofing, especially concerning the placement of schemes (VRP #28) and interactions with Android UI lifecycle (VRP #41, #47).
*   **Race Conditions:** Analyze timing issues related to slow navigations, scrolling, tab switching, and renderer crashes that could lead to stale or incorrect URL display (VRP #41, #47, #318).
*   **RTL/Bidi/Long URL Handling:** Test edge cases related to Right-To-Left characters, bidirectional text, and extremely long URLs that might cause misrendering or incorrect origin display (VRP #5139, #1618, #5977, #1856).
*   **Hostname/IP Heuristic Bypasses:** Can the `IsCanonicalizedHostCompliant` check or the IP address component counting logic be bypassed to force navigation for inputs that should be queries, or vice-versa?
*   **`view-source:` / `blob:` / `filesystem:` Interactions:** How does `ParseForEmphasizeComponents` handle nested or complex URLs within these schemes? Can the inner URL parsing be manipulated?
*   **External Protocol Handling:** How are redirects to external protocols handled in relation to the displayed origin in dialogs (VRP #254)?
*   **Interaction with `SecurityStateTabHelper`:** How resilient is the security indicator logic to receiving potentially incorrect parsed URL information from `AutocompleteInput`?

**Related VRP Reports:**

*   VRP #28 (VRP.txt L24): Security: Android: URL spoofing in address bar if scheme is later in URL (Fixed: `40072988`)
*   VRP #41 (VRP.txt L38): Security: Android address bar hidden after slow navigation finishes, if slow nav is initiated on page load (Fixed: `379652406`)
*   VRP #47 (VRP.txt L45): Security: Android address bar URL spoof if page is scrolling and tab is switched (Fixed: `343938078`)
*   VRP #254 (VRP.txt L254): Security: Origin spoof in external protocol dialogs via server-side redirect to external protocol (Fixed: `40055515`)
*   VRP #290 (VRP.txt L290): Security: Web Share dialog URL is incorrectly elided in Android (Fixed: `40061104`)
*   VRP #318 (VRP.txt L318): URL Spoof after crash (Fixed: `40057561`)
*   VRP #5139 (VRP2.txt L5139): Security: URL Spoofing via Bidirectional Domain Names
*   VRP #1618 (VRP2.txt L1618): Security: RTL character in URL flips domain and path (Android 4.2 and earlier)
*   VRP #5977 (VRP2.txt L5977): Security: UI spoofing using a very long URL
*   VRP #1856 (VRP2.txt L1856): Security: URL bar spoofing on Android with a very long URL
*   VRP #4286 (VRP2.txt L4286): Security: WebView and Chromium based browser Omnibar Spoofing with Race Condition
*   VRP #4900 (VRP2.txt L4900): Security: URL bar spoofing via download redirect
*   VRP #5140 (VRP2.txt L5140): Security: Address spoofing in Omnibox with HTTPS lock

**Related Wiki Pages:**

*   `url_utilities.md` (if exists)
*   `url_formatter.md` (if exists)
*   `security_state.md` (if exists)
*   `navigation_request_security.md` (if exists)
