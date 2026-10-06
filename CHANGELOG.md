# Changelog

## v1.1.1 — 2026-10-06 (staged; not published)

- Five quick BOTH-button taps now request ESP32 deep sleep, powering down Wi-Fi and the processor instead of only blanking the display. DASH remains the wake button; deep sleep makes the device unreachable until wake.
- An explicit five-tap sleep request is not canceled by an incoming peer heartbeat during the same loop.
- Preserve the prior four-tap dictionary delay, one-minute display-only idle behavior, DASH-held safety check, and wake-press suppression.
- Corrected project discovery for both packaged macOS releases (`project/` beside the `.app`) and development apps launched from the repository's `dist/`; show a helpful message if files are missing.
- Renamed the desktop utility in its user-facing title to **PMFT — Pocket Morse Flash Tool**. Existing app/executable filenames remain compatible with the release workflow.
- v1.1.0 battery estimate work remains included; charging remains board-hardware-managed.

### Verification and qualification

- Native C++ tests: all 14 groups passed, including the five-tap deep-sleep request regression test.
- PlatformIO build matrix: all 9 environments passed (`device_a`, `device_b`, 4 MB/16 MB, portrait/landscape, and `wokwi`). The only emitted warning was TFT_eSPI's expected missing touch-controller pin warning.
- WebAssembly battery simulator build was not verified locally for v1.1.0; CI must rebuild and pass the full simulator suite.
- Windows PMFT is built by the native Windows release runner; it was not built on this Mac.
- Physical DASH wake, immediate wake after five taps, paired reconnection, battery calibration, sleep current, and charge LED behavior have not been tested on the actual hardware.
- macOS PMFT 1.1.1 app and pairing helper built with PyInstaller 6.22.3 / Python 3.14.7. `codesign --verify --deep --strict` and archive `unzip -t` passed.
- Packaged-layout project discovery test passed against an extracted archive (`project/` resolved beside `.app`). The app has not been interactively launched or tested with physical boards; end-to-end upload remains unverified.

This is an experimental communications project and has not been independently security-audited. It is not suitable for emergency, safety-critical, or high-stakes use.

## v1.1.0 — 2026-10-06 (superseded staged build)

- Added the approximate battery estimate, periodic GPIO14/GPIO34 sampling, simulated battery readings, and deep-sleep shutdown of the TFT backlight and sensing circuit.
- All 9 configured PlatformIO targets and all 14 native test groups passed for the v1.1.0 source.
- WebAssembly battery simulator scenarios and physical battery/sleep behavior were not verified. See [v1.1.0 release notes](docs/releases/uploader-v1.1.0.md).

## 2026-10-05 — Deep-sleep wake reliability

- Configure DASH/GPIO35 as an RTC input before arming ESP32 EXT0 active-low deep-sleep wake, then restore its normal digital GPIO function after wake/reboot.
- Do not enter deep sleep while DASH is held. Ignore the DASH wake press until it is released so waking cannot accidentally invoke the normal DASH action.
- After a previously linked peer disappears, wait five minutes; reconnection cancels that countdown. Once asleep, the device is unreachable until DASH wakes it and startup reinitializes ESP-NOW.

### Verification

- `npm test`: passed all 11 native test groups and 15 WebAssembly scenarios, including peer recovery restarting the deep-sleep grace period.
- `.venv/bin/pio run -e device_a -j 1`: passed; generated a 4 MB-target firmware image. The only compiler warning was TFT_eSPI's expected notice that touch support has no `TOUCH_CS` pin configured.
- The macOS arm64 v1.0.0 ZIP was extracted and its app passed `codesign --verify --deep --strict`.
- Physical-board deep-sleep entry, DASH wake, and two-device reconnection were not verified.
