# Changelog

## v1.1.0 — 2026-10-06 (staged; not published)

- Show an approximate battery percentage beside the signal indicator on original LILYGO TTGO T-Display boards. The browser simulator uses clearly simulated values.
- Sample battery voltage periodically using the board's GPIO14-controlled ADC path on GPIO34; keep sampling short and non-blocking, and disable the sensing circuit between readings.
- Turn off and hold the TFT backlight and battery-sense enable inactive before deep sleep. Charging remains managed by the board's charger hardware.
- Preserve the existing always-reachable awake-state radio and heartbeat behavior; no Wi-Fi power-save or light-sleep changes were introduced.
- Document the estimate's limitations, hardware-managed charging, and the distinction between the charger LED and firmware-controlled outputs.

### Verification and qualification

- PlatformIO: all 9 configured firmware targets succeeded (A/B, 4 MB/16 MB, portrait/landscape, and Wokwi) using `pio run ... -j 1`.
- Native tests: all 14 test groups passed, including battery curve boundaries, invalid readings, scheduled ADC enable/disable, non-blocking sampling, sleep shutdown, and both screen orientations.
- WebAssembly simulator: **not verified for v1.1.0**. The checked-in generated bundle was stale, and local Emscripten compilation stalled. The CI quality gate must rebuild the bundle and pass the new simulated-battery scenarios before public publication.
- Physical board checks: battery estimate calibration, ADC values on actual board revisions, deep-sleep current, and LED behavior were not verified on hardware.
- Windows app: not built locally on this Mac. The release workflow builds it on a native Windows runner after the v1.1.0 tag is pushed.
- macOS arm64 app and pairing helper were freshly built with PyInstaller 6.22.3 / Python 3.14.7. The app was set to version 1.1.0, ad-hoc signed and verified with `codesign --verify --deep --strict` from a clean temporary copy; the 20 MB archive passed `unzip -t`.
- Archive SHA-256: `bac9c5b446b85de6353d0e3288b4e401200ff41db9c1eacc7861854b349076c6`.
- The application was packaged but not interactively launched or tested against attached hardware.

This is an experimental communications project and has not been independently security-audited. It is not suitable for emergency, safety-critical, or high-stakes use.

## 2026-10-05 — Deep-sleep wake reliability

- Configure DASH/GPIO35 as an RTC input before arming ESP32 EXT0 active-low deep-sleep wake, then restore its normal digital GPIO function after wake/reboot.
- Do not enter deep sleep while DASH is held. Ignore the DASH wake press until it is released so waking cannot accidentally invoke the normal DASH action.
- Document the actual power-state contract: after a previously linked peer disappears, the device waits five minutes; reconnection cancels that countdown; once asleep it is unreachable until DASH wakes it and startup reinitializes ESP-NOW.

### Verification

- `npm test`: passed all 11 native test groups and 15 WebAssembly scenarios, including peer recovery restarting the deep-sleep grace period.
- `.venv/bin/pio run -e device_a -j 1`: passed; generated a 4 MB-target firmware image. The only compiler warning was TFT_eSPI's expected notice that touch support has no `TOUCH_CS` pin configured.
- The macOS arm64 v1.0.0 ZIP was extracted and its app passed `codesign --verify --deep --strict`.
- Physical-board deep-sleep entry, DASH wake, and two-device reconnection were not verified.
