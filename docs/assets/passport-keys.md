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

Settings are ordered **Screen → Passport configuration → File transfer → Pairing system →
Clear Bluetooth pairing**, with four visible rows and scrolling.

- **Screen**: adjust brightness and idle dim/off timers; zero disables a timer.
  UP/DOWN adjusts, short OK saves, hold OK cancels. After screen-off, the first
  button press only wakes the screen; release before operating.
- **File transfer**: connect to saved Wi-Fi while keeping Bluetooth usable.
  UP/DOWN selects Enable/Disable; short OK applies, `>` marks the cursor and `*`
  the running state. Hold OK returns while sharing remains enabled. The device
  shows the current URL/QR and browser count only when the service is ready.
  No saved Wi-Fi: configure it in the hotspot first. After failure select Enable
  to retry. Restart defaults to disabled; starting either configuration mode ends sharing.
- **Pairing system**: manually select the global macOS (first-use default) or
  Windows setting. UP/DOWN previews, OK saves, hold OK returns; `*` marks the
  saved system. Failed saves retain it. Changes cancel actions and release keys.
- **Clear Bluetooth pairing**: when changing computers, clear pairing and forget
  Passport Keys on the old computer. Shortcuts, macros, scenarios and the selected
  system survive clearing pairing and restart.

## Web configuration

1. Open **Settings → Passport configuration**, select **Device hotspot** or **WiFi**,
   then choose **Cancel** or **Enable**. Device hotspot pauses Bluetooth until
   configuration closes; WiFi keeps Bluetooth connected. Enabling configuration
   ends file transfer.
2. For **Device hotspot**, join the displayed network and open `http://192.168.4.1`.
   For **WiFi**, a saved working network is required; use a browser in the same LAN
   and open the device’s displayed IP. Failed connections show no configuration
   address; hold OK to return and retry. No saved Wi-Fi disables Enable.
3. Edit the home badge, Wi-Fi network, scenarios and keyboard/media steps; empty
   steps disable an action. **Set as current** selects the active scenario.
   Select a physical key and click/hold gesture, then use **Record** or **Manual
   selection** to edit a keyboard step. **Save all settings**, **Exit configuration** and
   **Restore defaults** stay visible at the top. Click **Save all settings** once
   to save all edits, then hold OK on the device or click **Exit configuration** to close
   configuration. Clearing Wi-Fi also takes effect only after saving. All three modules
   save together; a failed save keeps the saved settings unchanged. Keep the page
   open and retry if saving cannot be confirmed.

Left/right **Cmd/Ctrl** sends the corresponding Command on macOS or Ctrl on Windows,
including macro steps and combinations with Shift. Fixed modifiers and recorded
keys retain their meaning; the default Command+A mapping remains fixed.

**Home badge** sets identity, avatar, UTC offset and 12/24-hour time; names and
labels allow up to 16 supported characters. Select or enter a 2.4 GHz Wi-Fi
network, test it through the device hotspot, then click **Save all settings**;
testing does not save. WiFi mode allows editing saved credentials but disables
scanning/testing to preserve the active connection; network edits apply after exit. The saved
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

## Local browser transfer

Enable **Settings → File transfer** and open the displayed `http://<IP>/transfer`
(or scan its QR) on devices in the same network. Name each browser, select one
recipient or all online devices, then send text or share files for recipients to
receive. At most four browser sessions and eight shared files per sender are
available. Keep the sender page open and phones in the foreground. Refreshing or
closing it removes its shared source; Passport never stores or relays content.

Transfers use local WebRTC without internet or cloud services. Guest-network
isolation, firewall rules or browser candidate handling can prevent direct
connections; retry after correcting the network. This LAN page exposes no
keyboard or Wi-Fi management APIs. Closing transfer ends browser RTC sessions.

If the page reports that WebRTC is unavailable, the browser cannot establish a
direct connection. Check whether a WebRTC extension allows the displayed Passport
address, or try another browser. Peers without WebRTC remain visible but are marked unavailable
for transfers; camera and microphone access are not required.

Receivers verify SHA-256 before saving. A browser with a secure context and a
supported file picker writes chunks directly; ordinary HTTP uses a bounded
browser download (128 MiB total on desktop, 64 MiB on mobile, per page). Pending
receives and retained downloads share this budget. Click **Save file**
after verification. Download status confirms handoff to the browser, not disk
persistence; compare saved-file hashes for acceptance. Limits are conservative
and browser-dependent; 100 MiB is the desktop acceptance target, not a promise
for every phone. Cancel or clear finished records to release receive capacity.

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
