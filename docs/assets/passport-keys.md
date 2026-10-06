[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys

A configurable Bluetooth keyboard for the three-button AI Passport. No resident
computer software is required.

## Use

Pair with **Passport Keys** in your computer's Bluetooth settings. UP/DOWN selects
**Start control**, **Scenario list** or **Settings**; short OK enters, hold OK
returns or cancels the current action. Enter Start control to send keys.

| Button | Single click | Double click | Triple click | Hold |
| --- | --- | --- | --- | --- |
| UP | Esc | None | None | Hold left Option |
| DOWN | Backspace | Left Command+A | None | Hold Down arrow |
| OK | Enter | None | None | Return / cancel |

Settings are ordered **Screen → Pairing system → Hotspot configuration → Clear
Bluetooth pairing**.

- **Screen**: adjust brightness and idle dim/off timers; zero disables a timer.
  UP/DOWN adjusts, short OK saves, hold OK cancels. After screen-off, the first
  button press only wakes the screen; release before operating.
- **Pairing system**: manually select the global macOS (first-use default) or
  Windows setting. UP/DOWN previews, OK saves, hold OK returns; `*` marks the
  saved system. Failed saves retain it. Changes cancel actions and release keys.
- **Clear Bluetooth pairing**: when changing computers, clear pairing and forget
  Passport Keys on the old computer. Shortcuts, macros, scenarios and the selected
  system survive clearing pairing and restart.

## Web configuration

1. Open **Settings → Hotspot configuration** and enable it; Bluetooth pauses.
2. Join the hotspot using the displayed name/password, then open `http://192.168.4.1`.
3. Edit the home badge, Wi-Fi network, scenarios and keyboard/media steps; empty
   steps disable an action. **Set as current** selects the active scenario.
   Select a physical key and click/hold gesture, then use **Record** or **Manual
   selection** to edit a keyboard step. **Save all settings**, **Exit hotspot** and
   **Restore defaults** stay visible at the top. Click **Save all settings** once
   to save all edits, then hold OK on the device or click **Exit hotspot** to resume
   Bluetooth. Clearing Wi-Fi also takes effect only after saving. All three modules
   save together; a failed save keeps the saved settings unchanged. Keep the page
   open and retry if saving cannot be confirmed.

Left/right **Cmd/Ctrl** sends the corresponding Command on macOS or Ctrl on Windows,
including macro steps and combinations with Shift. Fixed modifiers and recorded
keys retain their meaning; the default Command+A mapping remains fixed.

**Home badge** sets identity, avatar, UTC offset and 12/24-hour time; names and
labels allow up to 16 supported characters. Select or enter a 2.4 GHz Wi-Fi
network, test it, then click **Save all settings**; testing does not save. The saved
Wi-Fi password appears when the page opens; use the eye button to hide or show
it. Until time is available, the home shows `--:--`.

**Export complete configuration** backs up the badge, Wi-Fi and all scenarios,
including the Wi-Fi password in plain text; keep the file private. JSON import
loads all three modules into the editor; save to apply. Single-scenario export
shares keys only. Older v2–v5 files import scenarios while preserving the badge
and Wi-Fi edits. Existing device settings migrate automatically; new complete
files require the updated workbench.

**Restore defaults** resets the badge, Wi-Fi and scenarios after saving. Screen,
pairing-system and Bluetooth pairing settings remain outside this configuration.

## Build and flash

Activate **ESP-IDF 5.5.3** using the [environment guide](../development/engineering/environment-setup.md).
Run from the repository root, with the powered device connected by a USB data cable:

```sh
./tools/validate.sh
python -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201  # Replace with the detected port.
python -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

This application-only update preserves NVS when the installed partition layout is
compatible. For first installation or an intentional complete refresh, replace
the final line with `0x0 build/FoloToy-AI-Passport-full.bin`; **this resets saved
settings and pairing**. See the [flashing/data policy](../development/engineering/firmware-layout.md#flashing-and-stored-data).

## For agents

Start with [AGENTS.md](../../AGENTS.md), the [AI development guide](../development/ai-guide.md)
and the required [Passport skills](../../skills/README.md).
Application code lives in `main/main.c` and `main/keyboard/`; the webpage is in
`main/web/`. Reuse `components/bsp` for board logic. Preserve stored-data
compatibility and follow the [font guidance](../../assets/README.md#passport-keys-fonts)
for device text. Host tests and the [LVGL preview](../../tests/keypad_preview/) live
in `tests/`; run the static gate while editing and the complete gate before delivery.
