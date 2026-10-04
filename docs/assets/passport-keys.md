[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys

Passport Keys turns the three-button ESP32-C3 Passport into a Bluetooth keyboard
with configurable action sequences and scenarios. It needs no resident computer
software. The target is ESP-IDF 5.5.3, 8 MB Flash, no PSRAM and a 240 x 320 display.
The device interface is Chinese; menu names below are translated into English.

## Quick start

1. Install verified firmware using the flashing instructions below. The device
   starts in **Start control**, showing the current scenario and its mappings.
2. Connect to **Passport Keys** in the computer's Bluetooth settings. Just Works
   pairing requires neither a six-digit passkey nor confirmation on Passport.
3. Open a text editor: UP sends Esc, DOWN sends Backspace, OK sends Enter;
   hold UP for left Option, or double-click DOWN for left Command+A.
4. Hold OK for **500 ms** to open the menu. **Scenario list** changes scenarios;
   **Settings** opens hotspot configuration or clears Bluetooth pairing.

Clearing pairing does not delete scenarios. If the computer caches an older
keyboard name or bond, clear pairing on Passport and forget it on the computer.

## Buttons, details and execution

```text
Start control (default startup)
└── Hold OK → main menu
    ├── Start control
    ├── Scenario list → UP/DOWN select, OK save and activate
    └── Settings
        ├── Hotspot configuration → SSID, password, web address
        └── Clear Bluetooth pairing → cancel / confirm
```

UP/DOWN select items with wrapping; short OK enters or confirms. Hold OK returns
one level and does nothing in the main menu. Only Start control sends computer
keys. OK hold is reserved globally and has no configurable binding.

| Physical key | Single click | Double click | Hold |
| --- | --- | --- | --- |
| UP | Esc | Disabled | Hold left Option |
| DOWN | Backspace | Left Command+A | Hold Down arrow |
| OK | Enter | Disabled | Return, fixed |

Each scenario contains eight independent actions. Singles wait **180 ms** after
release for the double-click window. Holds trigger after **500 ms**; holding the
second press of a double click becomes a hold without sending a double click.
Existing saved mappings migrate into the Default scenario; updates retain them.

An action has a user-editable title and description. Its subtitle is the actual
sequence, for example `Command+A → Command+C`. Start control replaces the mapping
list with full-screen details during execution, then restores it **500 ms after
completion**. A single-step hold completes on physical release. Titles use 28 px;
subtitles and descriptions use 16 px. Long content wraps and ends with an ellipsis
when it exceeds its visible region. The web editor provides a browser-font preview.

Each click step presses a keyboard chord or media control for approximately
**40 ms**, releases all reports, then waits before the next step. The default gap
is **80 ms**, configurable to **0–5000 ms**; the final step has no extra delay.
A single-step UP/DOWN hold stays pressed until physical release, with repeats
controlled by the computer. A multi-step hold executes once; releasing its physical
key does not truncate or repeat it.

While an action runs, new command gestures are ignored, never queued or replayed.
Holding OK cancels remaining steps, releases reports and opens the menu. Page,
scenario and link changes, stale/overflowed input, or a failed report also stop
execution. Failed steps are not retried automatically; completed steps cannot be
undone by the device. Media controls require the computer's Consumer subscription
and are unavailable in Boot protocol.

The ADC ladder recognizes one physical button at a time; assign computer chords
to a single action. After 60 seconds without physical input, backlight drops to
12%; the next press restores 80%.

## Configure actions and scenarios over Wi-Fi

1. Hold OK, choose Settings, then hotspot configuration.
2. Join the displayed `Passport-Keys-XXXX` network with the fixed WPA2 password
   **`12345678`**, also shown on screen. Only one Wi-Fi client is allowed.
3. Open `http://192.168.4.1`. All HTML, CSS, JavaScript and coverage data are embedded.
4. Select a scenario to edit, or add a blank scenario, copy, rename or delete one.
   Editing does not activate it; **Set as current** activates it when saved.
   At least one scenario must remain. Deleting the active scenario selects the first
   remaining one. The device remembers its saved active scenario across reboot.
5. Choose UP/DOWN/OK and a single, double or hold action. Set its title and description,
   then add keyboard/media steps. Record one chord per step or choose keys manually;
   move/delete steps and edit the delay. No steps means disabled. OK hold is read-only.
6. Save. Invalid text, capacity overflow or a failed NVS commit leaves the prior
   device configuration active and retains the browser's unsaved edits.
7. Hold OK on Passport or click Exit hotspot. Wi-Fi and HTTP stop, BLE resumes and
   the device returns to Settings without reboot. Return to the menu and Start control.

Limits: **8 scenarios**, **64 steps per action**, and **6 KiB total compact storage**.
The web editor shows usage; individual maximums are not simultaneous guarantees.
Scenario names and titles allow 16 Unicode characters, descriptions 64. Names must
be nonempty and unique. Descriptions may contain newlines. The generated inventory
supports 21,364 glyphs, including source-supported basic Han characters, printable
ASCII and selected punctuation/symbols. Unsupported characters, including emoji
and vertical kana repeat marks U+3031/U+3032, are rejected explicitly. See the
[font assets](../../assets/README.md#passport-keys-fonts).

Recording uses `KeyboardEvent.code`, preserves modifier sides and ignores repeats.
Cancel, blur, hiding the page, changing scenarios/tabs, or editing step structure
cancels recording. Fn, input methods, system shortcuts and browser-reserved chords
may be intercepted; use manual selection. Browser previews use local fonts and do
not establish exact device typography.

BLE pauses for the entire hotspot session. Web pairing deletion first exits the
hotspot and requires confirmation on the device before removing the bond.

## Keys and Bluetooth compatibility

- Standard keyboard usages `0x04–0x73`, excluding Power `0x66`, including F1–F24
  and the numeric keypad. One chord allows six distinct ordinary keys.
- Eight left/right modifier bits for Control, Shift, Option/Alt and Command/GUI;
  modifier-only chords are supported.
- Volume up/down, mute, play/pause, previous track (`0xB6`) and next track (`0xB5`).

Apple Fn/Globe, vendor keys, brightness/backlight/power controls, text input and
multiple-computer switching remain unsupported. Keyboard usages follow the
[USB HID Usage Tables](https://www.usb.org/sites/default/files/hut1_21_0.pdf).

One computer's bond is retained. Changing computers requires clearing pairing and
forgetting the old record. Just Works retains encryption and bonding but lacks
MITM identity verification at initial pairing. Sending requires a connection,
encryption, a bond, a 16-byte key and keyboard notification subscription; it does
not require the MITM `authenticated` flag. The HID service remains on encrypted
GATT level 2 because the pinned IDF version's level-3 Boot declaration is invalid.

## Development and configuration contract

Start with [AGENTS.md](../../AGENTS.md), the [AI guide](../development/ai-guide.md)
and the five required [Passport skills](../../skills/README.md). Continue the
application branch and preserve local changes. Activate **ESP-IDF 5.5.3** following
[environment setup](../development/engineering/environment-setup.md).

| Subsystem | Responsibility |
| --- | --- |
| `main/main.c` | Worker, timestamped input, BLE/AP transitions, scenario commits, feedback and backlight |
| `main/keyboard/keypad_model.*`, `keypad_text.c` | Pure gestures, navigation, compact configuration, UTF-8 and glyph checks |
| `main/keyboard/keypad_runner.*` | Pure report sequencing, acknowledgement timing, holds, cancellation and failure |
| `main/keyboard/keypad_config.*`, `keypad_store.*` | Bounded streaming JSON and mutex-protected NVS persistence |
| `main/keyboard/keypad_web.*`, `main/web/` | Hotspot lifecycle and local action/scenario editor |
| `main/keyboard/keypad_ui.*`, `assets/fonts/` | Reused LVGL objects, details/list screens and verified font assets |

Version 2 JSON has exactly `version`, `active_profile` (zero-based) and `profiles`.
Each profile has `name` and eight `bindings`, ordered UP single/double/hold,
DOWN single/double/hold, OK single/double. Each binding has `title`, `description`
and `steps`; each step has `type` (1 keyboard, 2 media), `modifiers`, `keys`,
`consumer` and `delay_ms`. Empty steps disable the action. The final delay is zero.
Unknown/duplicate fields, invalid integer literals, repeated/unsupported keys,
unsupported glyphs and over-limit values are rejected.

NVS namespace `passport_keys` stores `config_v2` as a compact bounded blob. A valid
v2 wins; otherwise valid `config_v1` is migrated to Default and retained as a
fallback. Failed migration commits still use the old mappings in RAM and retry
on boot. Missing/corrupt configurations use defaults without erasing NVS or bonds.
Only a successful NVS commit replaces the active configuration.

| Request | Result |
| --- | --- |
| `GET /api/config` | `{config, session}`, streamed in bounded chunks |
| `POST /api/config` | Strict version-2 validation and commit |
| `POST /api/exit` | Empty body; exit hotspot and return to Settings |
| `POST /api/forget` | Empty body; request device confirmation after hotspot exit |

POST requires the GET-provided `X-Session`. API Host must be `192.168.4.1`, optionally
`:80`; CORS is disabled. Request bodies are limited to **32 KiB**, sockets to three
and Wi-Fi clients to one. Parsing uses small streaming buffers rather than a full
JSON tree or large task-stack configuration. Configurations reload after hotspot exit.

Static web assets stream in **512-byte blocks**. Config GET encodes the committed
store directly under its mutex, with no extra 6 KiB snapshot; the mutex is released
even when transmission fails. The reader cannot call store APIs or retain its
pointer. POST still uses a bounded heap candidate, so failed validation/allocation
or NVS commits leave the original configuration intact.

The configuration AP keeps optional Wi-Fi transmit/receive code in Flash instead
of shared instruction/data RAM. TCP send queues are **2,880 bytes per socket**
(two MSS segments), trading peak throughput for memory headroom. These changes
retain all scenario, step, text and capacity limits. Send/allocation failures log
free heap and largest block without logging configuration contents or sessions.

Preserve the 24-event input queue and 250 ms stale-input cancellation. BLE commands
remain serialized on NimBLE with timeout-safe buffer ownership. Button callbacks
only enqueue edges; all LVGL access holds `bsp_lvgl_lock()`. UI caching tracks
configuration revision and visible feedback rather than copying the configuration.
The original hardware-test demo is retained as reference and is not compiled.

## Validation, previews and flashing

```sh
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh --static
./tools/validate.sh
cmake -S tests/keypad_preview -B build/preview
cmake --build build/preview
mkdir -p build/preview/screens
build/preview/keypad_preview build/preview/screens
```

The host gate covers gestures, sequencing, UTF-8/glyph inventory, JSON, NVS migration
and failures, BLE, HTTP and browser helpers. HTTP checks exercise the embedded
assets against a transport limited to 512-byte sends, read a full 6 KiB config
with heap allocation disabled, and verify send failures, lock release, POST errors
and startup rollback. It compares browser defaults/capacity with
firmware-generated fixtures. The actual LVGL preview checks glyph lookup and
renders six pages, five detail states, a long scenario list and long overview
titles, then repeats 100 detail/overview transitions in the firmware's 24 KiB
built-in allocator. The application uses explicit styles and disables unused
automatic themes. These checks do not establish radio, operating-system timing
or physical display compatibility.

The 2026-10-04 validation with ESP-IDF 5.5.3 and tracked defaults measured:

| Resource | Measured bound |
| --- | --- |
| Application image / factory partition | 7,151,056 / 8,323,072 bytes |
| Linked internal RAM / linker capacity | 167,786 / 321,296 bytes |
| Remaining internal RAM before runtime allocations | 153,510 bytes |
| 64-bit host LVGL pool | Recorded peak allocation 17,432 bytes; largest remaining block 3,840 bytes |
| Compact configuration | Exactly 6,144 bytes accepted; overflow rejected |
| Streaming JSON input | Exactly 32,768 bytes accepted; overflow rejected |

The MAP's RAM remainder is not the runtime free heap. Font bitmaps/descriptors
remain in Flash; full configuration candidates use bounded heap allocations.
The memory optimization frees **19,396 bytes** of linked internal RAM compared
with the previous 187,182-byte build, removes the 6 KiB GET snapshot, and halves
the configured per-socket send queue. The earlier board measurement was 66,160
free bytes at startup and 22,044 after AP start, before client requests; that
firmware's config GET allocation failed during webpage loading. The optimized
firmware still needs new on-device measurements and hotspot acceptance.
Startup and hotspot start/stop logs report free heap, largest block, minimum heap
and worker stack watermark. Maximum-configuration hotspot operation and repeated
hotspot cycles still require measurements on the board.

The full gate retains verified `build/FoloToy-AI-Passport-full.bin` and matching
images/ELF/MAP/manifest in `build/firmware/<full-image-sha256>/`. Verify the chosen
archive with `python3 tools/archive_firmware.py verify build/firmware/<full-image-sha256>`.
Generated builds, previews and private logs stay ignored and are not published.

For a blank device or intentional refresh, write verified merged `full.bin` at
**`0x0`**. Its padding can reset NVS, including mappings and pairing. To preserve
settings, confirm the existing partition table and write the matching application
at its actual application offset, normally **`0x10000`**. Never write app-only
images at `0x0`. No original-firmware backup or full-chip erase is required.
Follow the [flashing/data policy](../development/engineering/firmware-layout.md#flashing-and-stored-data)
and obtain authorization before writing.

On-device acceptance must check actual chord order and gaps, both hold semantics,
busy-input suppression, OK cancellation, disconnect/reconnect without replay,
scenario save/reboot/migration, Chinese rendering and long-text ellipsis, and ten
hotspot cycles with stable heap/stack. Report **Build**, **Host tests**, **Device tests**
and **Unverified** separately. Previous basic Mac input validation does not validate
these new features. Logs omit keys, pairing secrets and raw report contents.
