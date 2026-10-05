# Pocket Morse Communicators v1.0.0

The first public release of firmware and a desktop flashing/pairing utility for two original LILYGO TTGO T-Display ESP32 boards. This is an experimental personal-communications project, not an emergency or safety-critical product.

## Highlights

- Compose and exchange Morse-encoded text over a direct ESP-NOW link using per-pair authenticated encryption.
- Keep a flash-backed inbox, retry unacknowledged messages, and retain the draft after a failed send.
- Use display idle without turning off the radio; after a previously linked peer remains disconnected for five minutes, deep sleep powers down the radio until DASH (GPIO35) wakes and reboots the device.
- Configure A/B roles and pair the two devices with the included desktop utility.

## Downloads and setup

Download the archive matching your computer from the assets attached to this GitHub release. Each archive contains the desktop utility, pairing helper, firmware source, README, and changelog. Install PlatformIO Core and make `pio` available on `PATH`; the utility builds and flashes the selected firmware locally.

Use two original TTGO T-Display boards. Verify each board's flash size before choosing the 4 MB or 16 MB target. Pair both devices together using the desktop utility; pairing creates a new random key and provisions it over USB. Do not share firmware dumps, pairing data, or device storage images publicly.

## Verification and limitations

- Automated checks passed on the release source: 11 native test groups, 15 WebAssembly scenarios, and a PlatformIO build for `device_a` (4 MB target).
- This workstation verified only the `device_a` 4 MB target. The tag-triggered GitHub release workflow runs the A/B, 16 MB, landscape, and Wokwi build matrix plus the test suite, and only publishes release assets after those checks succeed.
- Physical two-device messaging, flash persistence across power loss, and deep-sleep entry/wake/reconnection have not been verified for this release. Test these on the exact boards before relying on them.
- The browser simulator does not model real ESP-NOW radio, physical flash, TFT optics, or ESP32 sleep hardware.
- Encryption has not been independently audited. Secure Boot and flash encryption are not enabled; a person with physical access may extract stored keys. Do not use this project for high-stakes secrets.
- The macOS app is ad-hoc signed, not Apple-notarized; macOS may require the user to approve opening it. Windows may display an untrusted-app warning.
