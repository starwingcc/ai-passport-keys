[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys

Passport Keys turns the three-button ESP32-C3 Passport into a Bluetooth keyboard.
It starts in a dedicated Chinese command console and needs no resident computer
software. The target is ESP-IDF 5.5.3, 8 MB Flash, no PSRAM and a 240 x 320 screen.

## Quick start: using the device

1. Install verified Passport Keys firmware using the flashing instructions below.
   The command console is the default startup page.
2. Connect to **Passport Keys** in the computer's Bluetooth settings. Just Works
   pairing requires neither a six-digit passkey nor numeric confirmation on Passport.
3. Wait for the console to show a connected keyboard, then open a text editor.
   A short UP press sends Esc, a short DOWN press sends Backspace, and a short OK
   press sends Enter. Hold UP for Option; double-click DOWN for Command+A.
4. Hold OK for **500 ms** to open the main menu. Settings offers hotspot mapping
   configuration and Bluetooth bond deletion.

If the computer caches a different name from older firmware, clear the bond on
Passport, forget this keyboard on the computer, and connect again. Clearing the
Bluetooth bond does not delete key mappings.

## Buttons and navigation

```text
Command console (default startup)
└── Hold OK → main menu
    ├── Command mode → command console
    └── Settings
        ├── Hotspot configuration → SSID, password, web address
        └── Clear Bluetooth pairing → cancel / confirm
```

UP/DOWN select an item; a short OK press enters or confirms. Holding OK returns
one level and does nothing in the main menu. Only the command console sends
computer keys. OK hold is reserved globally and cannot be mapped.

| Physical key | Single click | Double click | Hold |
| --- | --- | --- | --- |
| UP | Esc | Disabled | Hold left Option |
| DOWN | Backspace | Left Command+A | Hold Down arrow |
| OK | Enter | Disabled | Return, fixed |

There are eight configurable actions. Singles wait **180 ms** after release for
the double-click window. UP/DOWN holds press their mapping at **500 ms** and
release it when the physical key is released; the computer controls repeat speed.
A second press held to the hold threshold becomes a hold without sending a double click.
Option remains pressed until UP is released. These defaults apply when no valid
mapping is saved; existing saved mappings are retained after a firmware update.

Page and connection transitions cancel pending gestures, release reports and wait
for physical keys to be released. Stale input is discarded without replay. The
ADC ladder recognizes one physical key at a time; assign computer chords to one
action rather than pressing several physical keys together. After 60 seconds
without physical input, backlight drops to 12%; the next press restores 80%.

## Configure mappings over Wi-Fi

1. Hold OK, enter Settings, then choose hotspot configuration.
2. Join the displayed `Passport-Keys-XXXX` network using the fixed WPA2 password
   **`12345678`**, also shown on the screen. Only one client is allowed.
3. Open `http://192.168.4.1`. HTML, CSS and JavaScript are embedded in firmware.
4. Choose UP/DOWN/OK and edit a single, double or hold action. Record a chord or
   choose ordinary keys and left/right modifiers manually. OK hold stays read-only.
5. Save the mapping to the device. A failed save leaves the existing device configuration active.
6. Hold OK on Passport or click exit in the web page. The device stops the web
   server and Wi-Fi, resumes BLE and returns to Settings without rebooting.
   Return to the main menu and enter command mode to use the saved mapping.

BLE is paused throughout the hotspot session. The web page can also request bond
deletion: it first exits the hotspot, then opens confirmation on Passport. The
bond is deleted only after confirmation on the device.

Recording uses `KeyboardEvent.code` for physical positions and ignores auto-repeat.
Cancel, blur or a hidden page clears recording. System shortcuts, input methods,
Fn and browser-reserved combinations may be intercepted; use manual selection
when recording is unavailable. Unsaved edits do not affect the device.

## Supported keys and pairing

- Standard keyboard usages `0x04–0x73`, excluding Power `0x66`, including F1–F24
  and the numeric keypad.
- Left/right Control, Shift, Option/Alt and Command/GUI: eight modifier bits.
  One chord supports up to six distinct ordinary keys plus modifiers;
  modifier-only mappings are also allowed.
- Consumer controls: volume up/down, mute, play/pause, previous track (`0xB6`)
  and next track (`0xB5`). Consumer input needs its own notification subscription;
  Boot protocol sends keyboard reports only.

Apple Fn/Globe, vendor-specific keys, brightness/backlight/power controls, macros,
text input, multiple profiles and multiple-computer switching are outside the
supported scope. Standard usages are listed in the
[USB HID Usage Tables](https://www.usb.org/sites/default/files/hut1_21_0.pdf).

The device stores one computer's bond and advertises again after disconnecting.
Before switching computers, clear pairing in Settings and forget the old record
on the computer. Just Works retains encryption and bonding, but initial pairing
does not provide MITM identity authentication through numeric comparison; see the
[Bluetooth security specification](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/security-manager-specification.html).
Reports require a connection, encryption, a bond, a 16-byte key and keyboard
notification subscription. The MITM `authenticated` flag is deliberately not
required. The obsolete numeric-confirmation UI has been removed.

## Quick start: development

Read root [AGENTS.md](../../AGENTS.md) and the
[AI development guide](../development/ai-guide.md), check the five required
[Passport skills](../../skills/README.md), and use only the skill for the current
phase. Continue the current application branch and preserve local changes.
Clients using repository-local skill links can run
`python3 tools/install_passport_skills.py` initially; other clients should follow
the skill guide for their installation method. Human-only development does not
require AI client skills.

Install and activate **ESP-IDF 5.5.3** using the
[environment guide](../development/engineering/environment-setup.md). Host tests
need a C compiler, Python 3, Node.js and actionlint. Validation installs a verified
actionlint version when missing. Set `NODE=/absolute/path/to/node` to select a
local Node executable. Run these commands from the repository root:

```sh
source /path/to/esp-idf-v5.5.3/export.sh
python3 tools/install_passport_skills.py --check
./tools/validate.sh --static
./tools/validate.sh
```

The complete gate generates isolated configuration from `sdkconfig.defaults`,
runs host tests, builds and verifies the merged image, and outputs
`build/FoloToy-AI-Passport-full.bin`. Matching application, bootloader, partition
table, ELF, MAP and `manifest.json` are retained under
`build/firmware/<full-image-sha256>/`. Verify the selected archive before flashing
or decoding a crash:

```sh
python3 tools/archive_firmware.py verify build/firmware/<full-image-sha256>
```

Do not commit generated firmware, raw logs, local machine paths or temporary
handoff notes. Ignored `build/` holds local build and debugging output. Original
hardware-test demos remain as references and are not compiled into Passport Keys.

### Code map and extension points

| File or directory | Responsibility |
| --- | --- |
| `main/main.c` | Initialization, input queue, application worker, report release, BLE/hotspot transitions and backlight |
| `main/keyboard/keypad_model.*` | Navigation, gesture timing, configuration validation and HID encoding independent of ESP-IDF/LVGL |
| `main/keyboard/keypad_config.*` | Strict JSON parsing and serialization |
| `main/keyboard/keypad_store.*` | Mutex-protected current configuration and NVS commits |
| `main/keyboard/keypad_ble.*` | HID service, GATT handle discovery, security, subscriptions and serialized host commands |
| `main/keyboard/keypad_web.*`, `main/web/` | Bounded hotspot/HTTP lifecycle and mapping editor |
| `main/keyboard/keypad_ui.*` | Five Chinese LVGL pages, fonts and visible-state cache |
| `assets/fonts/`, `tools/generate_keypad_fonts.py` | Licensed font subset, generated LVGL fonts and glyph inventory |
| `tests/test_keypad_*`, `tests/keypad_preview/` | Host regressions, platform stubs and actual LVGL preview |
| `components/bsp/` | Reusable board drivers; release event appended to preserve existing enum values |

Preserve these constraints when extending the application:

- Button callbacks only update physical state and enqueue timestamped events.
  The application worker handles BLE commands, networking and storage. Queue
  capacity is 24; overflow or input older than 250 ms cancels gestures and
  synchronizes physical key state.
- NimBLE serializes GAP/GATT and report sending. Only the application worker
  calls BLE command APIs. A timed-out host command owns its arguments until
  completion; shared buffers cannot be reused early.
- Discover GATT handles through registration callbacks rather than hardcoding
  them. Preserve SDK registration/sync/reset callbacks. Advertising UUIDs need
  static lifetime for IDF retries. With `MAX_CONNECTIONS=1`, do not restart
  advertising after the controller allocates a connection but before CONNECT arrives.
- ESP-IDF 5.5.3 exports `ble_gap_conn_foreach_handle` without a public prototype;
  the local declaration matches this pinned version. Its HID level-3 Boot flags
  have a syntax defect, so use level-2 encrypted GATT without modifying the SDK.
  Recheck both points when upgrading IDF.
- Application LVGL access holds `bsp_lvgl_lock()`. Create the screen once and
  reuse it; visible-state caching excludes gesture timestamps.
- Keep testable behavior in the model and add host coverage. Extending supported
  keys requires matching C and JavaScript catalogs. Always reserve OK hold.
- Device-text edits require regeneration and coverage checks for **both font sizes**;
  see [font assets](../../assets/README.md#passport-keys-fonts) and the
  [Chinese font checklist](../development/engineering/coding-conventions.md#chinese-fonts-and-missing-glyphs).
- Persistent structures and strict JSON contracts are versioned. New fields or
  layout changes need explicit migration. Preserve fallback for invalid/unsupported
  configuration and keep the active configuration unchanged when NVS writes fail.

### Configuration and HTTP contract

Host JSON tests contain an unmodified copy of
[cJSON](https://github.com/DaveGamble/cJSON/tree/c859b25da02955fef659d658b8f324b5cde87be3),
pinned to commit `c859b25da02955fef659d658b8f324b5cde87be3`, matching ESP-IDF 5.5.3.
Its MIT license is in
[tests/third_party/cjson/LICENSE.txt](../../tests/third_party/cjson/LICENSE.txt).
Firmware uses IDF's JSON component, not the host copy. Font provenance and the
SIL Open Font License are recorded in the font asset guide above.

Version 1 contains only `version: 1` and eight `bindings`, ordered as UP
single/double/hold, DOWN single/double/hold and OK single/double. Each has `type`
(0 disabled, 1 keyboard, 2 consumer), `modifiers` (0–255), `keys` (up to six
distinct valid nonzero usages), and `consumer` (zero for keyboard/disabled).
Reject unknown/duplicate fields, a ninth binding, unsupported versions and invalid
combinations. Mappings use NVS namespace `passport_keys`, blob `config_v1`; BLE
bonding is stored separately. Missing or corrupt configuration uses defaults;
startup never masks errors by erasing NVS.

| Request | Result |
| --- | --- |
| `GET /api/config` | `{config, session}` for the current hotspot session |
| `POST /api/config` | Validate and save; success only after the NVS commit |
| `POST /api/exit` | Empty body; exit hotspot and return to Settings |
| `POST /api/forget` | Empty body; exit hotspot and request on-device confirmation |

POST requires the `X-Session` returned by GET. API Host must be `192.168.4.1`,
optionally with `:80`; CORS is not enabled. Request bodies are limited to 2048
bytes, HTTP sockets to three and Wi-Fi clients to one. Stop HTTP before releasing
Wi-Fi/netif. Reload configuration into the model after leaving the hotspot.

## Validation, preview and flashing

Static validation covers gesture boundaries/counter wrap, eight mappings,
navigation/input cancellation, report encoding, strict JSON, NVS failure
recovery, GATT registration/subscriptions, Just Works security, BLE pause/timeouts/
allocation failures, web recording and generated font cmaps. Platform stubs do
not establish radio behavior or operating-system compatibility.

After IDF resolves Managed Components, render the five actual LVGL pages:

```sh
cmake -S tests/keypad_preview -B build/preview
cmake --build build/preview
mkdir -p build/preview/screens
build/preview/keypad_preview build/preview/screens
```

Run from the repository root. Preview verifies actual glyph lookup, a missing-glyph
negative case and UI caching, and writes five PPM images. It needs CMake and the
pinned actual LVGL dependency. It needs no serial port and does not control the
computer's interface.

For a blank device or intentional complete refresh, flash the verified merged
`full.bin` at **`0x0`**. Its NVS padding can reset mappings and bonds. To preserve
configuration, first verify the device's existing partition table, application
offset and capacity, then write the matching application at its actual starting
offset, **`0x10000`** by repository default. Never write an application-only image
at `0x0`. Original-firmware backups and full-chip erases are not prerequisites.
Follow the [flashing and stored-data policy](../development/engineering/firmware-layout.md#flashing-and-stored-data)
and obtain authorization before writing to the device.

After activating IDF, enumerate ports and replace `PORT` with the selected
Passport port. For an authorized complete refresh:

```sh
python -m serial.tools.list_ports -v
python -m esptool --chip esp32c3 --port PORT write_flash --flash_size 8MB \
  0x0 build/FoloToy-AI-Passport-full.bin
```

For an authorized update preserving configuration, use the verified archive's
`FoloToy-AI-Passport.bin` at the checked application offset. Do not open arbitrary
Bluetooth/serial ports or reuse an old image left by a failed build.

The user has verified basic UP/DOWN/OK input and initial passkey-free pairing on
Mac. Later source changes still require their own device regression. Check hold
release, consumer keys, disconnect/reconnect without replay, every return path,
ten hotspot entry/exit cycles, mapping persistence after reboot, on-screen Chinese
and runtime heap/stack stability. Report Build, Host tests, Device tests and
Unverified separately.

### Troubleshooting entry points

| Symptom | First checks |
| --- | --- |
| Computer shows connected but keys are ignored | Device connection/encryption/handles/subscriptions; only the console sends input |
| Old name or repeated pairing failure | Clear the bond on Passport, forget the same keyboard on the computer and pair again |
| Waiting for the computer to enable the keyboard | Notifications, discovered handles and Boot/report protocol |
| Consumer keys unavailable | Consumer notification subscription; Boot protocol cannot send consumer reports |
| Advertising error 6 | NimBLE connection slots and delayed CONNECT first, then actual allocation evidence; do not assume heap exhaustion |
| Web page unavailable | Join the displayed hotspot, use `http://192.168.4.1` and check USB startup logs |
| Missing new Chinese glyphs | Regenerate both sizes, verify cmaps/widget fonts and inspect the real screen |

USB logs retain sync/handles, connection/security, HID ready transitions,
disconnects, skipped input and memory snapshots. Subscription/report details
use DEBUG logging; enable that level in a local diagnostic build when needed.
Do not log passkeys, encryption keys or report contents. Match the startup ELF
identity to a verified archive before decoding addresses. Keep raw logs only in
ignored local output directories.
