# Pocket Morse Communicators v1.1.0

Version 1.1.0 adds an approximate battery percentage for the original LILYGO TTGO T-Display and reduces firmware-controlled loads before deep sleep. It retains the existing ESP-NOW connection behavior while awake. This is an experimental personal-communications project, not an emergency or safety-critical product.

## Highlights

- Display a rounded, voltage-based 1-cell LiPo percentage next to the peer signal indicator.
- Sample the original board's GPIO34 battery input through its GPIO14-enabled divider; power down the sensing path between scheduled samples.
- Add simulated battery readings to the browser UI for portrait and landscape layout checking.
- Turn off the TFT backlight and battery-sense enable before deep sleep. The built-in charger remains hardware-managed.

## Download and install

Choose the macOS or Windows archive attached to this release and extract the complete archive. Keep the desktop app, `provision_pair` helper, and `project/` directory together. Install PlatformIO Core and make `pio` available on `PATH`; the utility builds and flashes firmware locally. Use the 4 MB firmware target unless the exact board's flash capacity has been verified as 16 MB. Flashing the 16 MB partition layout onto 4 MB hardware can make the device unusable until recovered.

Pairing provisions a randomly generated pair key to the two devices over USB. Store replacement/recovery information safely; the app does not provide a cloud backup. Consult the packaged README and project README for the complete setup, wiring, and control guidance.

## Battery and charging notes

The percentage is only a rough voltage estimate, not a fuel-gauge measurement. Cell chemistry, temperature, age, load, and recent charge/discharge history affect accuracy. Firmware does not detect USB power, report authoritative charge state, or control charge current/limits. Charging is handled by the board's charger IC. Its charge-status LED is also hardware-controlled and may remain active when the ESP32 is asleep and USB is connected.

The browser simulator's 82% and 57% values are fixtures and are not measurements. Simulator behavior does not establish physical ADC calibration, RF reliability, flash persistence, or sleep current.

## Verification and known limitations

- All 9 local PlatformIO targets passed: A/B, 4 MB/16 MB, portrait/landscape, and Wokwi.
- All 14 native test groups passed, including battery conversion and scheduled ADC behavior.
- The macOS arm64 GUI app and pairing helper were freshly built with PyInstaller 6.22.3 / Python 3.14.7. The app was ad-hoc signed and passed `codesign --verify --deep --strict`; the resulting 20 MB ZIP passed `unzip -t`.
- The v1.1 WebAssembly simulator bundle and its new battery scenarios were **not verified locally** because Emscripten compilation stalled. The CI quality gate must pass before publication.
- Physical battery calibration, deep-sleep current, and exact-board LED behavior were not tested. Validate these on the hardware revision in use.
- Encryption has not received an independent audit; Secure Boot and flash encryption are not enabled. Physical access may expose stored keys. Do not use this prototype for high-stakes secrets.
- macOS application is ad-hoc signed and not notarized; Windows may display an untrusted-app warning.

macOS archive SHA-256: `bac9c5b446b85de6353d0e3288b4e401200ff41db9c1eacc7861854b349076c6`.

## Upgrade

Flash the same A/B role and verified flash-size target as before. Do not change partition layout casually on paired devices; migration and preservation of existing stored data across partition changes have not been established. Keep a secure record of the pair key and re-pair only when intentionally replacing/resetting devices.
