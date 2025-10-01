# Security Analysis of chrome/browser/extensions/crx_installer.cc

## 1. Overview

The `CrxInstaller` is arguably one of the most critical security components in the entire extensions subsystem. It is the final gatekeeper that stands between a downloaded CRX file (or an unpacked directory) and its installation into a user's profile. It is responsible for a comprehensive gauntlet of checks, including cryptographic signature verification, manifest validation, security policy enforcement, blocklisting, and user consent confirmation. A vulnerability in `CrxInstaller` could completely undermine the extension security model, potentially allowing for the silent installation of malicious extensions.

Its responsibilities include:
-   Orchestrating the unpacking of the CRX file via the `SandboxedUnpacker`.
-   Verifying that the unpacked extension matches a set of pre-established expectations (ID, manifest, version).
-   Running a series of pre-installation checks for policy, requirements, and blocklisting.
-   Handling the user-facing permission prompt (`ExtensionInstallPrompt`).
-   Committing the verified extension to the user's profile.
-   Setting the correct permission state (e.g., withholding host permissions) based on user choice.

## 2. The Installation Gauntlet: A Multi-Stage Security Flow

The `CrxInstaller` process is best understood as a sequential series of security gates. The installation is aborted if any gate fails.

### Stage 1: Sandboxed Unpacking
The process begins when `InstallCrxFile` is called. It immediately hands off the CRX file to the `SandboxedUnpacker`.
-   **Security**: This is the first and most important line of defense. The CRX file, which is untrusted input, is parsed and unpacked in a sandboxed utility process. This isolates the browser process from potential memory corruption vulnerabilities in the CRX parsing or unzipping code.
-   `OnUnpackSuccess` is the callback invoked upon successful and safe unpacking.

### Stage 2: Expectation Verification (`CheckExpectations`)
Once the CRX is unpacked, `OnUnpackSuccessOnSharedFileThread` is called, which immediately runs `CheckExpectations`. This is a critical security check against substitution or "bait-and-switch" attacks.
-   **`expected_id_`**: The installer is almost always created with an expected extension ID. This check ensures the ID derived from the public key in the CRX *exactly* matches the `expected_id_`. This prevents a Man-in-the-Middle attacker from substituting a malicious extension for the one the user intended to download.
-   **`expected_manifest_`**: For installations that bypass the user prompt (like those from `WebstoreInstaller`), the installer holds a copy of the manifest the user originally approved. `AllowInstall` verifies that the manifest from the CRX matches this approved manifest. This prevents an attacker from showing a benign permissions prompt to the user but delivering a CRX with a high-privilege manifest.
-   **`expected_version_`**: This is used to prevent accidental or malicious version downgrades.

### Stage 3: Pre-Install Checks (`OnInstallChecksComplete`)
After expectations are met, `CheckInstall` initiates a series of parallel checks.
-   **`PolicyCheck`**: Enforces enterprise policies, such as `ExtensionInstallAllowlist` and `ExtensionInstallBlocklist`. This is a critical gate for managed environments.
-   **`BlocklistCheck`**: Checks the extension's ID against the Safe Browsing blocklist. This is a vital, dynamic security mechanism that protects users from known-malicious extensions, even if they are hosted on the Web Store.
-   **`RequirementsChecker`**: Checks for unmet technical requirements (e.g., NPAPI, which is now defunct). While less of a security check now, it prevents the installation of extensions that cannot function correctly.

### Stage 4: User Confirmation (`OnInstallPromptDone`)
If all automated checks pass, the installer proceeds to `ConfirmInstall`. For a typical interactive installation, this shows the `ExtensionInstallPrompt`.
-   **Security**: This is the point of final user consent. The user sees the permissions requested by the extension and must explicitly approve them.
-   **Withholding Permissions**: The prompt's result (`DoneCallbackPayload`) determines the subsequent action. Crucially, if the user chooses to accept but withhold permissions, the payload result is `ACCEPTED_WITH_WITHHELD_PERMISSIONS`. This result is plumbed into `UpdateCreationFlagsAndCompleteInstall`, which sets the `Extension::WITHHOLD_PERMISSIONS` flag. This flag is the mechanism that enables the entire runtime host permissions security model.

### Stage 5: Final Installation (`CompleteInstall`)
Once approved, `CompleteInstall` is called on the file thread.
-   It moves the unpacked extension from the temporary directory into the final, permanent location inside the user's profile.
-   `ReloadExtensionAfterInstall` then loads the extension into the `ExtensionRegistry`.
-   Finally, `ReportSuccessFromUIThread` calls `PermissionsUpdater::GrantActivePermissions`, which formally grants the approved set of permissions to the extension, persisting them in `ExtensionPrefs`.

## 3. Key Security Mechanisms & Defenses

1.  **Sandboxing**: Unpacking the CRX in a separate, sandboxed process is the single most important defense against memory corruption bugs in the file parsing code.
2.  **Identity Verification (`expected_id_`)**: This is the primary defense against network-level substitution attacks.
3.  **Manifest Verification (`expected_manifest_`)**: This is the primary defense against UI-based "bait-and-switch" attacks.
4.  **Blocklisting**: Provides a dynamic, server-updated defense against known-bad actors.
5.  **Off-Store Hardening**: The requirement that non-webstore installs must be explicitly allowed (e.g., developer mode) or be a user-initiated drag-and-drop (`was_triggered_by_user_download`) is a powerful mitigation against drive-by installs from malicious websites.
6.  **Permission Model Integration**: The installer correctly sets the `WITHHOLD_PERMISSIONS` flag, acting as the bridge between the user's choice at the install prompt and the long-term enforcement of the runtime host permissions model.

## 4. Potential Attack Vectors & Security Risks

1.  **Bypass of Expectation Checks**: Any logic flaw that allows an installation to proceed even if `CheckExpectations` fails would be critical. This would open the door to extension substitution attacks.
2.  **Incomplete Pre-Install Checks**: If any of the `PreloadCheckGroup` checks (Policy, Blocklist) were to be skipped or their results ignored, a forbidden extension could be installed.
3.  **TOCTTOU on Filesystem**: There is a window between when the CRX is unpacked to a temporary directory and when it's moved into the profile. If an attacker with local filesystem access could modify the contents of the unpacked directory during this window, they could alter the extension's code after it has been verified. The use of a secure temporary directory owned by the browser process makes this difficult, but not impossible.
4.  **Logic Flaw in User Consent**: A bug in how `OnInstallPromptDone` interprets the result from the prompt could lead to permissions being granted when the user intended to withhold them, or an installation proceeding when the user cancelled it.

## 5. Conclusion

The `CrxInstaller` is a mature, security-hardened component that implements a robust, multi-stage validation process. It serves as the final authority on whether an extension is safe to be placed on a user's system. Its security relies on the strict and unforgiving sequence of its checks: sandboxed unpacking, identity and manifest verification, policy and blocklist enforcement, and finally, faithful adherence to the user's consent decision. Any modification to this class must be considered highly security-sensitive.