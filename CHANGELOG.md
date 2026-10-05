# Changelog

Notable implementation, user-facing, and release changes are recorded here. See the [v1.0.0 release notes](docs/releases/uploader-v1.0.0.md) for download guidance and qualification status.

## 2026-10-05 — Deep-sleep wake reliability

- Configure DASH/GPIO35 as an RTC input before arming ESP32 EXT0 active-low deep-sleep wake, then restore its normal digital GPIO function after wake/reboot.
- Do not enter deep sleep while DASH is held. Ignore the DASH wake press until it is released so waking cannot accidentally invoke the normal DASH action.
- Document the actual power-state contract: after a previously linked peer disappears, the device waits five minutes; reconnection cancels that countdown; once asleep it is unreachable until DASH wakes it and startup reinitializes ESP-NOW.
- Prepared the macOS arm64 v1.0.0 uploader package. GitHub publication remains a separate tag-triggered release action.

### Verification

- `npm test`: passed all 11 native test groups and 15 WebAssembly scenarios, including peer recovery restarting the deep-sleep grace period.
- `.venv/bin/pio run -e device_a -j 1`: passed; generated a 4 MB-target firmware image. The only compiler warning was TFT_eSPI's expected notice that touch support has no `TOUCH_CS` pin configured.
- The macOS arm64 ZIP was extracted and its app passed `codesign --verify --deep --strict`.
- Physical-board deep-sleep entry, DASH wake, and two-device reconnection have not been verified on hardware.
