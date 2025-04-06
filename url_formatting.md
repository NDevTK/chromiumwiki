# Component: URL Formatting & Elision

## 1. Component Focus
*   **Functionality:** Handles the formatting, parsing, canonicalization, and elision of URLs for display purposes, primarily in the Omnibox (address bar) and other UI surfaces (e.g., Security Interstitials, Page Info Bubble, potentially Dialogs). Aims to present URLs clearly and securely, preventing spoofing.
*   **Key Logic:** Parsing URL components (`url_formatter::SegmentURL`), fixing potentially invalid user input (`url_formatter::FixupURL`), canonicalizing hostnames (`net::CanonicalizeHost`), handling Internationalized Domain Names (IDN) and Punycode, eliding parts of the URL for display (`url_formatter::ElideURL`), formatting origins for security display (`url_formatter::FormatUrlForSecurityDisplay`).
*   **Core Files:**
    *   `components/url_formatter/url_formatter.cc`/`.h`
    *   `components/url_formatter/elide_url.cc`/`.h`
    *   `net/base/url_util.cc`/`.h`
    *   `net/base/registry_controlled_domains/registry_controlled_domain.cc` (for TLD info)
    *   `net/base/net_util.cc` (hostname canonicalization)
    *   `chrome/browser/ui/views/location_bar/location_bar_view.cc` (integrates formatting for display)
    *   `components/omnibox/browser/autocomplete_input.cc` (uses formatting during parsing)

## 2. Potential Logic Flaws & VRP Relevance
*   **Elision Errors:** Incorrectly eliding parts of the URL, potentially hiding the true origin or displaying misleading information.
    *   **VRP Pattern (Incorrect Elision):** Web Share dialog URL incorrectly elided on Android (VRP: `40061104`). Long subdomains causing incorrect elision in download UI (VRP2.txt#8323, #8581) or Document PiP (VRP2.txt#4130). General long URL issues (VRP2.txt#5977, #1856). Blob URL elision (VRP2.txt#11681).
*   **IDN/Punycode Handling:** Errors in converting between Punycode and Unicode, or inconsistent handling, allowing homograph attacks. Failure to display Punycode when necessary for confusable scripts/characters.
    *   **VRP Pattern (Homograph/Confusable):** Using visually similar characters (e.g., Cyrillic 'а' vs Latin 'a') to spoof domains (VRP2.txt#12825). Using confusable Unicode hyphens (U+2010, U+2011) (VRP2.txt#16501). Using space-like characters (U+0F8C) (VRP2.txt#16428).
*   **RTL/Bidi Issues:** Incorrect display of URLs containing mixed right-to-left (RTL) and left-to-right (LTR) characters, potentially flipping the perceived host and path.
    *   **VRP Pattern (RTL Spoofing):** RTL characters combined with specific URL structures causing the domain to be hidden or spoofed, especially on Android (VRP2.txt#15139, #12423, #5139, #1618).
*   **Scheme Display Issues:** Incorrectly displaying or omitting the URL scheme, potentially misleading users about the connection's security (HTTP vs. HTTPS).
    *   **VRP Pattern (Delayed/Incorrect Scheme):** Scheme displayed incorrectly or with delay, especially on Android or after specific interactions (VRP: `40072988`, VRP2.txt#1652).
*   **Origin Formatting (`FormatUrlForSecurityDisplay`):** Errors in formatting origins specifically for security-sensitive UI (dialogs, prompts), potentially showing incorrect or misleading information.
    *   **VRP Pattern (Opaque Origin Display):** Chooser dialogs (Bluetooth/USB) showing empty origin for opaque initiators (VRP: `40061374`, VRP2.txt#8904). FedCM prompts showing `://` for opaque origins (VRP: `340893685`, VRP2.txt#13066).
*   **Parsing Interaction (`FixupURL`):** Errors in `FixupURL` when attempting to canonicalize user input might interact poorly with display logic.

## 3. Further Analysis and Potential Issues
*   **Elision Logic (`ElideURL`):** Audit the rules for eliding schemes, trivial subdomains (`www`), paths, queries, fragments. Are there edge cases with long components or specific character combinations that lead to incorrect elision?
*   **IDN/Punycode Conversion:** Review the usage of ICU libraries (`IDNToASCII`, `IDNToUnicode`). Is conversion applied consistently before display and security checks? Are all necessary confusable characters correctly identified and forced to Punycode?
*   **Bidi Algorithm Application:** How is the Unicode Bidirectional Algorithm applied to URLs for display? Are there edge cases or platform differences (especially Android) that could lead to spoofing?
*   **`FormatUrlForSecurityDisplay` Usage:** Identify all UI surfaces using this function. Does it always produce an unambiguous and correct origin representation, especially for complex cases like blob URLs, filesystem URLs, opaque origins?
*   **Interaction with Omnibox Parsing:** How does the formatting logic interact with `AutocompleteInput::Parse`? Can formatted URLs be misinterpreted upon re-parsing (e.g., if submitted from suggestions)?

## 4. Code Analysis
*   `url_formatter::FormatUrl`: Core function for general URL formatting.
*   `url_formatter::ElideURL`: Handles elision logic.
*   `url_formatter::FormatUrlForSecurityDisplay`: Specific formatting for security UI. Check handling of different schemes and opaque origins.
*   `url_formatter::FixupURL`: Attempts to canonicalize potentially malformed user input.
*   `net::CanonicalizeHost`: Hostname canonicalization.
*   `net::IDNTo*`: Punycode/Unicode conversion (uses ICU).
*   `base::i18n::WrapStringWithLTRFormatting / WrapStringWithRTLFormatting`: Bidi wrapping utilities, check usage context.

## 5. Areas Requiring Further Investigation
*   **Comprehensive IDN/Bidi Testing:** Test with a wide range of IDN homographs, mixed scripts, and RTL/Bidi sequences across different platforms (especially Android).
*   **Elision Edge Cases:** Test elision with extremely long subdomains, paths, queries, fragments, and combinations thereof. Test interaction with user edits in the omnibox.
*   **`FormatUrlForSecurityDisplay` Audit:** Review all call sites and ensure the output is appropriate and secure for each specific UI context (dialogs, interstitials, bubbles, etc.). Pay special attention to opaque/special origins.
*   **FixupURL Robustness:** Test `FixupURL` with malformed inputs aiming to confuse subsequent formatting or parsing steps.

## 6. Related VRP Reports
*   **Elision/Long URL:** VRP: `40061104` (Web Share Elision); VRP2.txt#8323, #8581 (Download UI Long Subdomain), #4130 (Doc PiP Long about:blank), #5977, #1856 (General Long URL), #11681 (Blob URL).
*   **IDN/Punycode/Homograph:** VRP2.txt#12825 (General Homograph), #16501 (Unicode Hyphen), #16428 (Tibetan Space), #12423 (German Sharp S).
*   **RTL/Bidi:** VRP2.txt#15139, #12423, #5139, #1618.
*   **Scheme Display:** VRP: `40072988`; VRP2.txt#1652.
*   **Origin Formatting:** VRP: `40061374` (Chooser Opaque), `340893685` (FedCM Opaque); VRP2.txt#8904, #13066.

*(See also [omnibox.md](omnibox.md), [navigation.md](navigation.md))*