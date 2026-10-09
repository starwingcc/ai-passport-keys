[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys

A configurable Bluetooth keyboard for the three-button AI Passport. No resident
computer software is required.

## For agents

Start with [AGENTS.md](../../AGENTS.md), the [AI development guide](../development/ai-guide.md)
and the required [Passport skills](../../skills/README.md). Application code lives
in `main/main.c` and `main/keyboard/`; webpages live in `main/web/`. Reuse
`components/bsp` for board logic and follow
the [font guidance](../../assets/README.md#passport-keys-fonts) for device text.
Tests live in `tests/`; run the static gate while editing and the complete gate
before delivery. Flashing, device testing and Git writes require the user's authorization.

## Use

Pair with **Passport Keys** in your computer's Bluetooth settings. UP/DOWN selects
**Start control**, **Scenario list** or **Settings**; short OK enters, hold OK
returns or cancels the current action. Enter Start control to send keys.

| Button | Single click | Double click | Triple click | Hold |
| --- | --- | --- | --- | --- |
| UP | None | Undo | Redo | Hold left Option/Alt, titled “Can be configured for voice input” |
| DOWN | Backspace | None | None | Select all and delete |
| OK | Enter | None | None | Return / cancel |

The initial scenario is **coding mode** (displayed as `coding` followed by the
Chinese word for mode). These defaults apply on first initialization and after
restoring defaults; updates retain saved scenarios. Select the matching
**Paired devices**: macOS uses Command+Z, Shift+Command+Z and Command+A;
Windows uses Control+Z, Control+Y and Control+A. Select-all is followed by
Backspace once. The voice-input title labels an Alt hold; configure the host's
voice-input shortcut as needed.

Settings are ordered **Screen → Paired devices → File transfer → Passport configuration →
Clear Bluetooth pairing**.

- **Screen**: adjust brightness and idle dim/off timers; zero disables a timer.
  UP/DOWN adjusts, short OK saves, hold OK cancels. After screen-off, the first
  button press only wakes the screen; release before operating.
- **File transfer**: connect to saved Wi-Fi while keeping Bluetooth usable.
  UP/DOWN selects Disable (top) or Enable (bottom); short OK applies, `>` marks the cursor and `*` the
  running state. Hold OK returns without disabling sharing. Open the displayed
  URL or QR after the service is ready. Configure Wi-Fi through the hotspot first
  if needed; select Enable to retry a failure. Restart or entering configuration ends sharing.
- **Paired devices**: retain A and B, with a separate macOS/Windows setting
  for each. Select an empty slot and press OK to add a host; hold OK cancels.
  Full slots are never overwritten automatically. Device actions contain
  **Switch device** and **Change operating system**. Switching keeps only one
  host connected; a sleeping host may need to reconnect from its Bluetooth settings.
  Selecting the connected host keeps that connection. Successfully saving
  a switch target opens Start control; input still waits for the selected host's
  connection and HID readiness.
- After pairing, **Operating system** opens immediately. UP/DOWN selects,
  OK saves, `>` marks the cursor and `*` the saved system. Keyboard, media and
  macros stay disabled until that host's system is saved successfully.
  A successful system save opens Start control. A connected host without a
  saved system shows an operating-system setup prompt at the top of that page.
  Editing pauses input; hold OK cancels and retains the previous system.
  A failed save retains the previous value and allows retry. Switching and restart
  restore each host's saved system. Older global system settings need one
  identity-specific confirmation after upgrading.
- Names are read from the host when available; unreadable, empty, unsupported
  or overlong names use **Device A/B**. Names never select an operating system.
- **Clear Bluetooth pairing**: confirmation clears both bonds, names and systems,
  then opens initial pairing again. Forget Passport Keys on the affected computers
  before pairing again. Shortcuts, macros and scenarios remain saved.
- **Pairing recovery**: if the computer requests a key missing on the device,
  the home and control screens ask you to forget/delete Passport Keys in the
  computer's Bluetooth settings and pair again. Repeated unsecured failures
  with no saved local pairing show a tentative old-pairing notice. The notice
  stays stable during retries and clears when a secure connection succeeds;
  local navigation remains available.

## Web configuration

1. Open **Settings → Passport configuration**, select **Device hotspot** or **WiFi**
   to start configuration directly. The hotspot pauses Bluetooth; WiFi keeps it connected.
   Both modes end file transfer.
2. For **Device hotspot**, join the displayed network and open `http://192.168.4.1`.
   For **WiFi**, a saved working network is required; use a browser in the same LAN
   and open the device's displayed IP. If connection fails, hold OK to return and
   retry. Configure Wi-Fi through the hotspot when no network is saved.
3. Edit the home badge, Wi-Fi network, scenarios and keyboard/media steps; empty
   steps disable an action. **Set as current** selects the active scenario.
   Select a physical key and click/hold gesture, then use **Record** or **Manual
   selection** to edit a keyboard step. Click **Save all settings** to apply all
   edits, then hold OK on the device or click **Exit configuration**. A failed
   save leaves the saved settings unchanged; keep the page open and retry.

Each keyboard or media action targets **All**, **Windows** or **Mac**. The device
runs All and matching actions using the saved pairing system, skipping other
actions and their waits. Adjacent Windows/Mac actions share a numbered step card;
move or delete the whole card, or edit/delete each branch. Bottom execution previews
show both systems with shared numbering. Device display previews and on-device
summaries follow the saved system. Limits count actual actions: 64 per gesture,
sharing 6 KiB across scenarios.

**Ctrl**, **Shift**, **Option/Alt** and **Command** are independent fixed modifiers.
Old left/right Cmd/Ctrl settings become fixed left/right Command when loaded or
imported; actions without a target apply to All. A hold with one executable action
lasts until release; multiple executable actions run once.

**Home badge** sets identity, avatar, UTC offset and 12/24-hour time. Names and
labels allow up to 16 supported characters; the clock shows `--:--` until time
is available. Wi-Fi requires a 2.4 GHz network. Both configuration modes support
scanning and connection testing; obtaining an IP passes the test without time
synchronization, and a test does not save credentials. In WiFi mode,
a test temporarily interrupts the page and restores the original network after
success, failure or timeout. Keep the page open while it retries; if the displayed
IP changes, reopen the new address. If recovery fails, use hotspot configuration.
Network edits apply after saving and exiting configuration.

**Export complete configuration** backs up the badge, Wi-Fi and all scenarios,
including the Wi-Fi password in plain text; keep the file private. Import loads
the editor; save to apply. Single-scenario export shares keys only and preserves
badge and Wi-Fi edits on import.

**Restore defaults** resets the badge, Wi-Fi and scenarios after saving. Screen,
per-device systems and Bluetooth pairing settings remain outside this configuration.

## Local browser transfer

Enable **Settings → File transfer** and open the displayed `http://<IP>/transfer`
(or scan its QR) on devices in the same network. Name each browser, select one
recipient or all online devices, then send text or share files for recipients to
receive. Text records show recipients and their confirmations; click **Copy** to
copy the text. At most four browser sessions and eight shared files per sender
are available.

Keep pages open and phones in the foreground. Refreshing or closing a page ends
its transfers, removes its shared files and discards unsaved received files.
Passport never stores or relays content. Transfer uses local WebRTC; camera and
microphone permissions are not required. Check network isolation, firewall rules
or WebRTC restrictions if devices cannot connect; try another browser if needed.

Progress appears below each file. After receipt and verification reach 100%,
**Save file** replaces **Cancel** on the right. Confirm the download in the browser.
Desktop and mobile pages each allow 1024 MiB total for pending receives, ongoing
receives and unsaved downloads; saving or canceling releases capacity. Actual
capacity depends on the browser and device. Where direct saving is supported,
choose a destination when receiving; the file is saved automatically on completion.

## Build and flash

Activate **ESP-IDF 5.5.3** using the [environment guide](../development/engineering/environment-setup.md).
Run from the repository root, with the powered device connected by a USB data cable:

```sh
./tools/validate.sh
python -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodemXXXX  # Replace with the detected port.
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
After a complete refresh, forget the old Passport Keys pairing on your computer
and pair again so both devices use the new pairing keys.
