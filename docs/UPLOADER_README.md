# PMFT — Pocket Morse Flash Tool (v1.1.1)

Thank you for trying Pocket Morse Communicators. This release is for **two original LILYGO TTGO T-Display ESP32 boards** (ST7789 display, GPIO0 and GPIO35 onboard buttons). It does not support the T-Display S3.

## Quick start

1. Install PlatformIO Core and make the `pio` command available on your `PATH`.
2. Keep PMFT (`PocketMorseUploader.app` on macOS or `PocketMorseUploader.exe` on Windows), `provision_pair` (or `provision_pair.exe`), and the `project/` folder together after extracting the archive.
3. Connect one board by USB, select its serial port and role (A or B), confirm its flash size, and build/upload. Repeat for the second board with the other role.
4. Connect both boards, select their serial ports in the pairing section, and choose **Generate key & pair A + B**. Pairing generates a random pair key and provisions it to both devices over USB.

Use the 4 MB target unless you have verified that the exact board has 16 MB flash. Never flash the 16 MB partition layout to a 4 MB board. Keep serial monitors closed while flashing or pairing. There is no recovery copy of the generated pair key; replacing a board requires a planned re-pair.

## Important limitations

- This is an experimental personal-communications project, not an emergency, safety-critical, or high-stakes system.
- Physical paired-device messaging, battery voltage calibration, sleep/wake on hardware, and reconnection have not been qualified for this release. Test with your exact boards before relying on it.
- The battery icon percentage is an approximate voltage-based estimate, not a fuel-gauge reading. Charging is managed by the board hardware; firmware does not monitor charge state or control charge current.
- When a board enters deep sleep, the TFT backlight and battery-sense circuit are disabled. The charger-status LED is hardware-controlled and may still indicate charging while USB is connected.
- The browser simulator and host tests do not model real ESP-NOW radio, display optics, or physical flash behavior.
- The encryption implementation has not had an independent security audit. Secure Boot and flash encryption are not enabled; physical access may expose stored keys.
- The macOS application is ad-hoc signed and not notarized. macOS may ask you to approve opening it; Windows may show an untrusted-app warning.

See `project/README.md` for wiring, controls, sleep behavior, battery estimate details, simulator instructions, and development workflows. See `RELEASE_NOTES.md` for this release's scope and verification results.
