# Component: URL Formatting & Elision

## 1. Component Focus
*   **Functionality:** Handles the formatting, parsing, canonicalization, and elision of URLs for display purposes, primarily in the Omnibox (address bar) and other security-sensitive UI surfaces (e.g., Security Interstitials, Page Info Bubble, Permission Prompts, Download UI, PiP windows). Aims to present URLs clearly and securely, preventing spoofing attacks.
*   **Key Logic:** Parsing URL components (`url_formatter::SegmentURL`), fixing potentially invalid user input (`url_formatter::FixupURL`), canonicalizing hostnames (`net::CanonicalizeHost`), handling Internationalized Domain Names (IDN) including Punycode conversion and confusable character detection (`net::IDNTo*`, skeleton matching), applying the Unicode Bidirectional Algorithm (Bidi) for display, eliding parts of the URL for brevity (`url_formatter::ElideURL`), and formatting origins/URLs specifically for security UI (`url_formatter::FormatUrlForSecurityDisplay`, `url_formatter::FormatOriginForSecurityDisplay`).
*   **Core Files:**
    *   `components/url_formatter/url_formatter.cc`/`.h`: Core formatting logic (`FormatUrl`, `FormatUrlForSecurityDisplay`).
    *   `components/url_formatter/elide_url.cc`/`.h`: URL elision logic (`ElideUrl`, `ElideHost`).
    *   `components/url_formatter/spoof_checks/`: IDN spoof checking logic (`idn_spoof_checker.cc`, `confusable_domains.cc`).
    *   `net/base/url_util.cc`/`.h`: Lower-level URL parsing utilities.
    *   `net/base/registry_controlled_domains/registry_controlled_domain.cc`: Used for determining eTLD+1.
    *   `net/base/net_util.cc`: Contains `net::CanonicalizeHost`, `net::IDNTo*`.
    *   `base/i18n/rtl.cc`: Bidi handling utilities.
    *   `chrome/browser/ui/views/location_bar/location_bar_view.cc`: Integrates formatting for Omnibox display.
    *   `components/omnibox/browser/autocomplete_input.cc`: Uses formatting/parsing during input processing.
    *   Various UI components call formatting functions (e.g., `DownloadItemModel`, `PermissionPromptBubbleBaseView`, `PictureInPictureBrowserFrameView`).

## 2. Potential Logic Flaws & VRP Relevance

Flaws in formatting, parsing, or display logic can be exploited for various UI spoofing attacks.

*   **Elision Errors / Long URL Issues:** Incorrectly eliding parts of the URL, especially with very long subdomains, paths, or queries, potentially hiding the true origin or displaying misleading information.
    *   **VRP Pattern (Origin Obscured):** Long subdomains causing the actual eTLD+1 to be elided or pushed out of view. Affects Omnibox (VRP2.txt#5977, #1856), Download UI (VRP2.txt#8323, #8581 - Android focus), Document PiP title (VRP2.txt#4130 - long `about:blank#...` URL). General long URL confusion (VRP2.txt#12072).
    *   **VRP Pattern (Incorrect Elision Logic):** Specific features incorrectly applying elision logic (e.g., Web Share dialog on Android - VRP: `40061104`). Elision issues with `blob:` URLs (VRP2.txt#11681).

*   **IDN / Punycode / Homograph Attacks:** Errors in converting between Punycode and Unicode, failure to detect confusable scripts/characters, or inconsistent handling allowing visually identical but distinct domain names (homographs).
    *   **VRP Pattern (Homograph/Confusable):**
        *   *Script Mixing:* Using visually similar characters from different scripts (e.g., Cyrillic 'а' vs Latin 'a') not caught by skeleton checks (VRP2.txt#12825).
        *   *Confusable Characters:* Failure to block or force Punycode for specific confusable characters that resemble standard ASCII separators or letters. Examples: Unicode Hyphens U+2010/U+2011 (VRP2.txt#16501), Tibetan Inverted Mchu Can U+0F8C rendered as space on Mac (VRP2.txt#16429).
        *   *Inconsistent Handling:* German Sharp S 'ẞ' incorrectly canonicalized to "ss" instead of "ß", leading to wrong domain resolution (VRP2.txt#12423).

*   **RTL / Bidi Manipulation:** Incorrect display of URLs containing mixed right-to-left (RTL) and left-to-right (LTR) characters, potentially flipping the perceived host and path, especially on platforms like Android where Bidi rendering might differ.
    *   **VRP Pattern (RTL Spoofing):** RTL characters/markers combined with specific URL structures causing the domain part to be hidden or parts of the path/query to appear as the domain (VRP2.txt#15139, #12423, #5139, #1618, #15140 - often Android-specific).

*   **Scheme Display Issues:** Incorrectly displaying or omitting the URL scheme, potentially misleading users about the connection's security (HTTP vs. HTTPS) or the nature of the URL (`javascript:`, `data:`, `blob:`).
    *   **VRP Pattern (Delayed/Incorrect Scheme - Android):** Scheme display lagging behind navigation, showing incorrect security state transiently (VRP: `40072988`; VRP2.txt#1652).
    *   **VRP Pattern (Special Scheme Display):** Incorrect or confusing display for `blob:` URLs in Omnibox (VRP: `1069246`; VRP2.txt#705778, #11681).

*   **Origin Formatting (`FormatUrlForSecurityDisplay`, `FormatOriginForSecurityDisplay`):** Errors in formatting origins specifically for security-sensitive UI (dialogs, prompts), potentially showing incorrect, missing, or misleading information.
    *   **VRP Pattern (Opaque Origin Display):** Chooser dialogs (Bluetooth/USB/Serial/HID) showing empty origin string (`://`) when initiated from an opaque origin (sandboxed iframe, data URL). `FormatOriginForSecurityDisplay` returns empty string for opaque origins. (VRP: `40061374`, `40061373`; VRP2.txt#8904, #9771). FedCM prompts showing `://` for opaque origins (VRP: `340893685`; VRP2.txt#13066). See [webusb.md](webusb.md), [fedcm.md](fedcm.md).

## 3. Further Analysis and Potential Issues
*   **Elision Logic (`ElideURL`, `ElideHost`):** Audit the rules for eliding schemes, trivial subdomains (`www`), user/pass, paths, queries, fragments. Are there edge cases with extremely long components (beyond simple length, e.g., many segments) or specific character combinations that lead to incorrect/misleading elision? How does this interact with fixed-width UI elements?
*   **IDN Spoof Checks (`idn_spoof_checker.cc`):** How comprehensive are the skeleton-based confusable character checks? Are all necessary scripts and characters covered? Is the list of restricted Unicode hyphens/spaces complete (VRP2.txt#16501, #16429)? How is script mixing detected? Is Punycode consistently enforced when spoofing is detected?
*   **Bidi Algorithm Application (`base::i18n::WrapStringWith*`):** How is the Unicode Bidirectional Algorithm applied to URLs for display? Are there sequences that can invert the visual order of host/path segments? Are there platform differences (especially Android) in rendering that enable spoofs? (VRP2.txt#15139, #12423).
*   **`Format*ForSecurityDisplay` Usage:** Identify all UI surfaces using these functions (Omnibox security chip, Page Info, Permission Prompts, Download UI, PiP titles, etc.). Does the output always provide an unambiguous and correct origin representation, especially for complex cases like blob URLs, filesystem URLs, opaque origins? Should opaque origins display the precursor origin in some cases?
*   **Interaction with Omnibox Parsing:** How does the formatting logic interact with `AutocompleteInput::Parse`? Can formatted/elided URLs be misinterpreted upon re-parsing (e.g., if submitted from suggestions or edited by the user)?

## 4. Code Analysis
*   `url_formatter::FormatUrl`: Core function for general URL formatting. Includes IDN decoding.
*   `url_formatter::ElideURL`, `ElideHost`, `ElideRectangle`: Handles elision logic based on available width and component importance.
*   `url_formatter::FormatUrlForSecurityDisplay`, `FormatOriginForSecurityDisplay`: Specific formatting for security UI. Key functions for preventing spoofing in dialogs/prompts. **Need robust handling of opaque origins.**
*   `url_formatter::FixupURL`: Attempts to canonicalize potentially malformed user input before parsing.
*   `IDNSpoofChecker` (`idn_spoof_checker.cc`): Performs checks based on ICU skeletons and character blocklists to detect potentially confusable IDNs. Needs up-to-date data and robust logic.
*   `net::CanonicalizeHost`, `net::HostStringToLower`: Hostname canonicalization.
*   `net::IDNToASCII`, `net::IDNToUnicode`: Punycode/Unicode conversion (uses ICU).
*   `base::i18n::*`: Bidi handling utilities, wrapping text with appropriate directionality markers.
*   `LocationBarView`: Uses formatting functions to update Omnibox display.

## 5. Areas Requiring Further Investigation
*   **Comprehensive IDN/Bidi Testing:** Test with a wide range of IDN homographs (especially involving less common scripts), mixed scripts, and complex RTL/Bidi sequences across different platforms (especially Android). Use tools like `generate_spoof_check_test_data.py`. Ensure all known confusable characters (hyphens, spaces, lookalikes) are handled.
*   **Elision Edge Cases:** Test elision with extremely long subdomains, paths, queries, fragments, and combinations thereof. Test interaction with user edits in the omnibox and varying window widths.
*   **`Format*ForSecurityDisplay` Audit:** Review all call sites. Ensure the output is appropriate and secure for each specific UI context (dialogs, interstitials, bubbles, etc.). **Define and implement secure handling for opaque origins.**
*   **Scheme Handling:** Ensure consistent and secure display of schemes, especially `blob:`, `filesystem:`, `data:`, `javascript:`, and during transitions (HTTP->HTTPS, redirects).

## 6. Related VRP Reports
*   **Elision/Long URL:** VRP: `40061104` (Web Share Elision); VRP2.txt#8323, #8581 (Download UI Long Subdomain), #4130 (Doc PiP Long `about:blank#...`), #5977, #1856 (General Long URL), #11681 (Blob URL), #12072 (General Long URL).
*   **IDN/Punycode/Homograph:** VRP2.txt#12825 (General Homograph), #16501 (Unicode Hyphen), #16429 (Tibetan Space U+0F8C), #12423 (German Sharp S ẞ).
*   **RTL/Bidi:** VRP2.txt#15139, #12423, #5139, #1618, #15140 (Spoofing via RTL/Bidi).
*   **Scheme Display:** VRP: `40072988`; VRP2.txt#1652 (Delayed/Incorrect Scheme Android).
*   **Origin Formatting:** VRP: `40061374`, `40061373` (Chooser Opaque); VRP: `340893685` (FedCM Opaque); VRP2.txt#8904, #9771 (Chooser Opaque), #13066 (FedCM Opaque).

## 7. Cross-References
*   [omnibox.md](omnibox.md)
*   [navigation.md](navigation.md)
*   [downloads.md](downloads.md)
*   [permissions.md](permissions.md) (Chooser dialogs)
*   [fedcm.md](fedcm.md)
*   [picture_in_picture.md](picture_in_picture.md) (Doc PiP title)