# Pocket Morse Communicators

Created by **Pilafdob**, Pocket Morse Communicators let two LILYGO TTGO T-Display ESP32 boards exchange **encrypted Morse messages** over ESP-NOW using only their onboard buttons. The browser-based simulator runs the same portable C++ core through WebAssembly.

## Controls

| Gesture | Compose mode | Incoming/reading mode |
| --- | --- | --- |
| Tap DOT / DASH | Append Morse symbol | Previous / next stored message while reading |
| Wait 0.5 seconds after symbol | Auto-confirm letter | — |
| Tap BOTH | Confirm pending letter; otherwise add a space | Reveal incoming message; tap again to return to compose |
| Hold DASH 0.8 s | Erase pending letter, or last letter of current word | — |
| Hold DOT 0.8 s | Erase entire last word, including a pending Morse letter | Immediately delete the message being read |
| Hold BOTH 0.8 s | Send current draft; retry after failure | — |
| Double-tap BOTH | Open inbox, including previously read saved messages | — |
| Triple-tap BOTH | Restore last sent text if draft is empty | — |
| Quadruple-tap BOTH | Open Morse dictionary | — |
| Dictionary DOT / DASH / BOTH | Previous page / next page / close | — |

Only the two onboard buttons are used. The top-left **LINK/UNLINK** state is an authenticated ping/reply freshness indication (every 2 seconds, expires after 6 seconds), not a continuous radio connection. The top-right ring fills for auto-confirm and button holds. The physical display deliberately omits packet IDs/attempt counters; the browser workbench shows those for debugging.

The draft remains on screen after failed delivery and can be retried without retyping. It is cleared only after an authenticated application-level receipt ACK. Recipient contents remain hidden until accepted. The top of the screen shows `nN mS` (new/saved counts) and `ROOM n` (estimated remaining message slots); additional messages can arrive while one is open. Double-tap BOTH from compose opens the oldest unread message, or the oldest saved read message when nothing is unread, without changing the draft. DOT/DASH browse all saved messages; `MSG X/Y` identifies the current position. Read messages remain browsable until deliberately deleted. There is no arbitrary message-count limit on the physical device: its dedicated flash filesystem reserves at least 64 KiB or 2% (whichever is greater) as a write-safety buffer. `ROOM` counts reusable deleted record slots plus conservative new 144-byte records below that buffer; it is an estimate, not a guarantee against storage faults or flash wear. Existing saved records remain readable even if an older firmware version had already used space now reserved; deleted slots remain reusable. If a write fails, the sender receives no receipt ACK. Deletion reuses a record slot, but reading does not require deletion or free space.

An ACK means the recipient device has successfully written and re-read the encrypted message record, **not** that a person has read it. The physical inbox survives ordinary restarts, but the composition draft and last-sent recall are RAM-only. The browser/Wokwi inbox is simulated in RAM. A power cut during a storage write, damaged flash, or an untested hardware fault can still cause failure; this is not a life-safety messenger.

## Build and browser lab

Install PlatformIO CLI and an Emscripten SDK providing `em++` on `PATH` (tested here with Emscripten 4.0.23 and PlatformIO 6.1.19). Node.js is also needed; there are no npm package dependencies.

```sh
pio run -e device_a -e device_b
pio run -e device_a_16mb -e device_b_16mb
pio run -e device_a_landscape -e device_b_landscape
npm run build
npm test
npm start
```

Open the URL printed by `npm start` (normally `http://127.0.0.1:4173`). It defaults to the **135×240 portrait** screen and offers a landscape-preview checkbox. It has two communicators, a packet trace, controls to drop text/ACK, corrupt encrypted frames, disconnect/reconnect, and built-in scenarios. `npm test` runs host C++ sanitizer tests plus WebAssembly scenarios. The simulator uses two independent instances of the C++ app and the same encrypted channel, but a RAM inbox rather than physical flash; its `ROOM` count is the RAM model's remaining slots, not physical flash capacity. Its transport has deterministic 80 ms latency, not actual RF propagation. Browser keyboard: `Q/E` for A DOT/DASH, `I/P` for B; simultaneous keys make BOTH.

## Hardware, flashing, and pairing

The original T-Display's onboard GPIO0 button is DOT and GPIO35 button is DASH. This matches LILYGO's listed GPIOs and factory sketch; the Button 1/2 numbering differs between those two references, but the pins are the same. GPIO0 is configured as an active-low input with pull-up; GPIO35 is input-only, active-low, and uses its board pull-up, so firmware uses plain `INPUT` there. Both switches are already routed on the board; no external button wiring is needed. Release GPIO0 while resetting or powering up, because holding it low at reset selects the ESP32 serial bootloader. GPIO27 is unused. The ST7789 screen is physically 135×240 in portrait. Firmware defaults to TFT_eSPI rotation 0 and a 135×240 sprite; separate `*_landscape` environments preserve rotation 1 and the 240×135 layout. Both use `Setup25_TTGO_T_Display.h`. The USB cables from the pictured kit are suitable for testing; no external button, LED, resistor, or buzzer is required.

**Flash size matters.** LILYGO documents a 4 MB original board and a 16 MB variant; the seller specification you supplied says 16 MB, but that is not a readout from the actual chip. Install `esptool` if needed, then check each attached board with `python3 -m esptool --port /dev/cu.YOUR_PORT flash_id` before using a 16 MB target. The regular `device_a`/`device_b` targets retain a 4 MB-safe partition table (about 2.4 MiB inbox partition); `device_a_16mb`/`device_b_16mb` use a 2 MiB firmware partition and a 13.94 MiB inbox partition. Filesystem metadata reduces usable message space. Each encrypted record is 144 bytes; the app does not hard-code a message limit. **Never flash a 16 MB partition table to a 4 MB device.**

Flash *different* roles on two physical boards, for example:

```sh
pio run -e device_a_16mb -t upload --upload-port /dev/cu.YOUR_A_PORT
pio run -e device_b_16mb -t upload --upload-port /dev/cu.YOUR_B_PORT
```

With both USB serial ports free of monitors, pair once:

```sh
python3 -m pip install esptool pyserial
python3 scripts/provision_pair.py --a /dev/cu.YOUR_A_PORT --b /dev/cu.YOUR_B_PORT
```

The tool verifies each board's role/MAC, initializes only a fully blank inbox partition, generates a fresh random 256-bit key on the USB host, sends it only to those two boards, and neither prints nor saves it. It refuses a previously used/corrupt partition rather than silently formatting messages. Firmware stores the pair key and anti-replay counters in ESP32 NVS; message contents are separately encrypted in LittleFS with a pair-derived key. Routine pairing refuses to overwrite existing data. Both boards must use the same `config::Channel` in `src/config.h` (default 1); no Wi-Fi router is needed. **Do not share the provisioning USB serial ports with a monitor.** If a device is replaced or either pair record is erased, stop and plan a deliberate recovery/re-pair of both devices; there is no recovery copy of the key. Updating an already-paired board from the older partition scheme has **not** been migration-tested: back up what matters and do not assume its key or messages will survive a partition-table change.

If macOS does not show the board as a `/dev/cu.*` serial port, identify its USB bridge before installing a driver. LILYGO's [T-Display repository](https://github.com/xinyuan-lilygo/ttgo-t-display) links CHxxx and CP210x drivers; the [CH9102 macOS link you supplied](https://github.com/Xinyuan-LilyGO/CH9102_Mac_Driver) is relevant only if your board actually uses that bridge. This project does not install drivers automatically.

## Security boundary

Application text is converted to Morse symbols and packed as binary dot/dash sequences **before** authenticated encryption. All packet types—text, receipt ACK, and link ping/pong—use Monocypher XChaCha20-Poly1305 with role-separated keys and durable transmit counters. The entire padded payload is encrypted at a fixed 109-byte size. ESP-NOW's unicast CCMP is enabled as an additional layer, with its own pair-derived keys. Malformed authentication tags, wrong pair keys, altered bits, reflection, and replayed frames are rejected. A radio listener can still see timing, MAC addresses, channel, and frame counts; encryption does not make the transmission invisible.

This is substantially safer than compiling one permanent common secret into every firmware image: each pair gets an independent random root key, provisioned over USB and stored locally. It is **not** unhackable. A compromised USB host, a stolen device whose flash/NVS can be read, or a future cryptographic flaw can expose a key. Message records are encrypted at rest, but because the storage key derives from the NVS root key, this does **not** defeat physical key extraction. The ESP32 build here does **not** enable Secure Boot or Flash Encryption. A more protective production design would provision secrets under secure boot + flash encryption and support authenticated key rotation; an ephemeral authenticated key exchange would also add forward secrecy, at the cost of more code and setup. No independent security audit or penetration test has been performed. Do not reuse this for high-stakes secrets without one.

## Wokwi and verification

`pio run -e wokwi` builds the single-device Wokwi firmware; `diagram.json` wires GPIO0 and GPIO35 (external 10 kΩ pull-up). `wokwi.toml` points to the built binary. Open this project in Wokwi and run `wokwi/smoke.yaml` to check button/mode behavior. Wokwi's current supported parts do not reproduce the exact ST7789 panel, and multi-device ESP-NOW is unavailable, so this target uses an encrypted in-memory peer and serial status output. It cannot validate the real RF path or TFT optics. Do not interpret a Wokwi build as a successful Wokwi runtime test.

Verified on this host: all nine two-button 4 MB/16 MB portrait, preserved landscape, and Wokwi firmware targets built after changing letter auto-confirm to 500 ms; 11 native test groups passed under AddressSanitizer/UndefinedBehaviorSanitizer, including 320 upstream AEAD known-answer vectors, 872 one-bit corruption probes, 10,000 parser probes, encrypted-inbox reboot/full/corruption/ACK checks, capacity counters and layout bounds; 14 WebAssembly scenarios passed, including the 500 ms auto-confirm threshold. Current portrait and landscape browser screenshots were inspected; the visible check button reported `13/13`, with no browser console errors. The browser and Wokwi do not exercise LittleFS. A physical flash-size readout, two-board exchange, LittleFS runtime, Wokwi runtime launch, and real TFT visual inspection have **not** been verified here.

Sources: [PlatformIO board](https://docs.platformio.org/en/stable/boards/espressif32/lilygo-t-display.html), [LILYGO T-Display pinout](https://github.com/Xinyuan-LilyGO/TTGO-T-Display), [ESP-NOW security](https://docs.espressif.com/projects/esp-idf/en/v4.4/esp32/api-reference/network/esp_now.html), [Monocypher AEAD](https://monocypher.org/manual/aead), [Wokwi supported hardware](https://docs.wokwi.com/getting-started/supported-hardware).
