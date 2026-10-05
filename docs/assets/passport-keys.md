[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys application guide

Passport Keys turns the three-button AI Passport into a Bluetooth keyboard with
configurable actions and scenarios. No resident computer software is required.

## Use

The pixel badge home shows identity, time, battery, Bluetooth status and the
active scenario under **SCENE**. UP/DOWN select **Start control**, **Scenario list**
or **Settings**; short OK enters and hold OK returns. Connect to **Passport Keys**
in the computer's Bluetooth settings, then enter Start control to send keys.
Use Scenario list to switch scenarios.

Default actions:

| Button | Single click | Double click | Triple click | Hold |
| --- | --- | --- | --- | --- |
| UP | Esc | None | None | Hold left Option |
| DOWN | Backspace | Left Command+A | None | Hold Down arrow |
| OK | Enter | None | None | Menu / return / cancel |

Hold OK to cancel an action. To change computers, use **Settings → Clear Bluetooth
pairing**, then forget the device on the old computer. This keeps your scenarios.

To configure the device:

1. Open **Settings → Hotspot configuration**, select **Enable** and confirm.
   Bluetooth disconnects temporarily.
2. Join the hotspot with the name and password shown on screen, then open
   `http://192.168.4.1` in a browser.
3. Edit scenarios, button gestures and keyboard/media steps; empty steps disable
   an action. **Set as current** chooses the active scenario. Save your changes.
4. Hold OK on the device or click **Exit hotspot**. Bluetooth resumes.

**Home badge** sets the user name, optional short label, robot/cat/astronaut/chip
avatar, fixed UTC offset and 12/24-hour format. Names and labels allow up to
16 supported characters; long text is reduced or abbreviated. Default time is
UTC+8 in 24-hour format; adjust the offset manually for daylight saving time.

**Sync phone/computer time** sets the clock from your browser's system time.
For automatic time after power-on, use **Scan nearby Wi-Fi** to select a 2.4 GHz
network, or enter a hidden network manually. Enter its password and click
**Test connection and time**. Results distinguish authentication, missing network,
timeout and time-service failures. Authentication can pass even if time sync fails.
Keep the browser connected to the Passport hotspot during testing; it retries brief
interruptions. Testing does not save: click **Save automatic-time network** afterward.
Passwords are visible by default; **Show password** toggles visibility. Saving keeps
the entered password; after a page reload, leave the blank field unchanged to keep
the saved password. **Clear saved network** removes it. The device synchronizes after
leaving the hotspot and at startup when the network is available; until it obtains
valid time, the home shows `--:--`.

**Export current scenario** shares the selected scenario; **Export all configuration**
backs up all scenarios. **Import JSON** loads the editor; review and save to write
the device. Scenario import/export and Restore defaults do not include badge
settings, Wi-Fi credentials or Bluetooth pairing.

## Build and flash

Use **ESP-IDF 5.5.3** and run from the repository root. See
[environment setup](../development/engineering/environment-setup.md) if needed.
For the installation under `build/tooling` (adjust paths for another installation):

```sh
export IDF_TOOLS_PATH="$PWD/build/tooling/espressif"
export IDF_PYTHON_ENV_PATH="$IDF_TOOLS_PATH/python_env/idf5.5_py3.12_env"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
source "$PWD/build/tooling/esp-idf-v5.5.3/export.sh"
PASSPORT_PY="$IDF_PYTHON_ENV_PATH/bin/python"
./tools/validate.sh
```

Connect the powered device with a USB data cable. In the same terminal, list ports
and replace the example below with the actual device port:

```sh
"$PASSPORT_PY" -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201
```

To **keep settings and pairing**, with the existing partition layout unchanged,
write only the verified application at **`0x10000`**:

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

For **first installation or an intentional complete refresh**, use the same command
with `0x0 build/FoloToy-AI-Passport-full.bin`. **This resets saved badge, scenario,
network and pairing data.** Never write the application-only image at `0x0`.
After flashing, reconnect Bluetooth and check the home, keys and scenario switching.

## For agents

Start with [AGENTS.md](../../AGENTS.md), the [AI development guide](../development/ai-guide.md)
and the required [Passport skills](../../skills/README.md). Preserve existing changes.

- `main/main.c`, `main/keyboard/`: application UI, interaction, Bluetooth and configuration.
- `main/web/`: hotspot configuration webpage.
- `assets/fonts/`, `tests/`: font assets, host tests and [LVGL preview](../../tests/keypad_preview/).

Keep reusable hardware logic in the BSP. Preserve stored-data compatibility and
check [font coverage](../../assets/README.md#passport-keys-fonts) for text changes.
Run `./tools/validate.sh --static` while iterating and the complete gate before delivery.
Report Build, Host tests, Device tests and Unverified separately. Obtain authorization
before flashing; commit or push only when requested.
