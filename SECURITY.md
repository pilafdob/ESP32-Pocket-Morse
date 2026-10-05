# Security Policy

## Supported versions

Security fixes are considered for the latest published release, the latest beta release, and the repository's default branch. Older releases and older beta builds are unsupported; update to the latest release before reporting an issue when possible.

## Report a vulnerability

Please report suspected vulnerabilities privately through [GitHub's private vulnerability reporting form](https://github.com/pilafdob/ESP32-Pocket-Morse/security/advisories/new), when available. If GitHub says the form is unavailable, open a public issue containing only a request for a private reporting channel; do not include vulnerability details there. The maintainer can enable private reporting in the repository's security settings or provide another private contact route. Do not post exploit details, proof-of-concept code, pair keys, message contents, or device dumps in a public issue, discussion, or pull request.

Include the affected release or commit, hardware/operating system where relevant, impact, and clear reproduction steps. Redact pair keys, serial output containing sensitive data, MAC addresses where they identify a device, and any personal information.

There is no bug bounty. The maintainer will make a best effort to acknowledge reports within seven days and coordinate a fix and disclosure timeline with the reporter. Please allow time for investigation; this is an early-stage, unaudited project.

## Security scope and limitations

Reports are in scope when they demonstrate a security impact in the ESP32 firmware, ESP-NOW protocol, USB pairing/provisioning utility, encrypted inbox, or distributed desktop uploader.

The project has not received an independent security audit. ESP32 Secure Boot and Flash Encryption are not enabled; physical extraction of flash/NVS may expose a pair key. The browser simulator uses an in-memory link and inbox and does not model real radio, physical flash, or hardware security. These documented limitations are not by themselves vulnerabilities; report a concrete way they can be exploited beyond the stated behavior.

Do not use this prototype for high-stakes secrets or life-safety communication.
