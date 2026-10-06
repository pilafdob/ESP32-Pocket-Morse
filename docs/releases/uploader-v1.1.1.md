# Pocket Morse Communicators v1.1.1

This staged update changes the five-tap sleep gesture and corrects project-folder discovery in the macOS flashing utility. The desktop utility's user-facing name is now **PMFT — Pocket Morse Flash Tool**. Existing executable and archive filenames remain unchanged for compatibility. Version 1.1.0's approximate battery estimate is included.

## Changes

- Tap BOTH five times quickly to enter ESP32 deep sleep. Wi-Fi and most of the chip power down; the communicator is offline and cannot receive messages until it is awakened by pressing DASH. This is deep sleep, not connected standby or light sleep.
- A peer heartbeat cannot cancel the user's explicit sleep request.
- Four taps still open the dictionary after the existing 800 ms disambiguation delay. Automatic one-minute display idle still turns off only the backlight, leaving the radio active.
- On wake, firmware suppresses the DASH wake press until release to prevent it triggering a normal input action. Deep sleep is not entered if DASH is already held.
- PMFT finds the bundled `project/` folder beside its `.app` in a release archive and also recognizes the repository root when run directly from a development `dist/` folder. The release archive must be fully extracted with its project directory and pairing helper kept alongside the app.
- The packaged-layout project discovery test passed against the extracted release archive; a real firmware upload has not been tested.
- User-facing desktop title changed to PMFT (Pocket Morse Flash Tool).
- Retains the approximate voltage-based battery percentage and hardware-managed charging notes from v1.1.0.

## Install

Download and extract the complete platform archive. Keep the PMFT app/executable, `provision_pair` helper, and `project/` directory together. Install PlatformIO Core and make `pio` available on `PATH`. For the 16 MB build, verify the actual flash chip first; do not use a 16 MB partition layout on 4 MB hardware.

## Verification status

- All 14 native test groups passed, including the five-tap deep-sleep request.
- All 9 PlatformIO targets passed (A/B, 4 MB/16 MB, portrait/landscape, and Wokwi).
- The macOS PMFT 1.1.1 app and pairing helper built with PyInstaller 6.22.3 / Python 3.14.7. The app passed strict ad-hoc signature verification and the 20 MB archive passed ZIP integrity testing.
- Do not treat this staged build as a fully qualified public release until CI passes the WebAssembly suite and native runner builds pass.
- WebAssembly battery simulator rebuild previously stalled locally; CI must rebuild it and pass scenarios before publication.
- Windows package must be built on the native Windows release runner.
- Actual hardware checks remain outstanding: five-tap deep-sleep entry, DASH wake/reconnection, battery estimate, sleep current, and charge LED behavior.
- This project is experimental and not independently security-audited; it is not suitable for safety-critical use.
