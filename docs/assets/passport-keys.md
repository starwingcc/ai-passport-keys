[简体中文](passport-keys.zh_CN.md) · **English**

# Passport Keys application guides

Passport Keys turns the three-button AI Passport into a Bluetooth keyboard with
configurable actions and scenarios. No resident computer software is required.

## Use

On startup, the device opens **Start control**. Connect to **Passport Keys** in
the computer's Bluetooth settings, then press the device buttons to send keys.

Default mappings:

| Button | Single click | Double click | Hold |
| --- | --- | --- | --- |
| UP | Esc | Disabled | Hold left Option |
| DOWN | Backspace | Left Command+A | Hold Down arrow |
| OK | Enter | Disabled | Open menu / return |

In menus, UP/DOWN select, short OK enters or confirms, and hold OK returns.
Use **Scenario list** to switch the active scenario. To change computers, use
**Settings → Clear Bluetooth pairing**, then forget the device on the old computer.
Clearing pairing does not delete scenarios.

To configure actions:

1. Open **Settings → Hotspot configuration**, select **Enable** and confirm.
   Bluetooth disconnects temporarily; the confirmation defaults to Cancel.
2. Join the hotspot using the name and password shown on screen, then open
   `http://192.168.4.1` in a browser.
3. Add or edit scenarios. Choose a button and gesture, set a title, and add
   keyboard or media steps. An empty step list disables the action. Use
   **Set as current** to select the startup scenario if needed, then save changes.
4. Hold OK on the device or choose **Exit hotspot** on the webpage. Bluetooth
   resumes; return to Start control to use the saved actions.

Up to **8 scenarios** and **64 steps per action** are supported. Scenario names
and action titles allow **16 characters**; the webpage shows total capacity.
OK hold is reserved for navigation and cancellation. A single-step hold stays
pressed until release; a multi-step hold runs once. New actions are ignored while
another runs; hold OK to cancel.

## Build and flash

Use **ESP-IDF 5.5.3**; see [environment setup](../development/engineering/environment-setup.md)
if it is not installed. Run these commands from the repository root. This example
uses the existing installation under `build/tooling`; adjust paths for other installations:

```sh
export IDF_TOOLS_PATH="$PWD/build/tooling/espressif"
export IDF_PYTHON_ENV_PATH="$IDF_TOOLS_PATH/python_env/idf5.5_py3.12_env"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
source "$PWD/build/tooling/esp-idf-v5.5.3/export.sh"
PASSPORT_PY="$IDF_PYTHON_ENV_PATH/bin/python"
./tools/validate.sh
```

The complete gate tests, builds and verifies the firmware. It produces
`build/FoloToy-AI-Passport.bin` and `build/FoloToy-AI-Passport-full.bin`.
Connect the powered device with a USB data cable, list serial ports, then set
`PASSPORT_PORT` to the device's port. The current macOS device uses `/dev/cu.usbmodem1201`;
if the listed port differs, use that value:

```sh
"$PASSPORT_PY" -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201
```

Run the flashing commands in this same terminal. In a new terminal, activate
ESP-IDF and set `PASSPORT_PY` again first.

### Update while keeping scenarios and pairing

With this project's existing partition layout unchanged, write only the application
at **`0x10000`**:

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

### First installation or complete refresh

Write the merged image at **`0x0`**. **This clears stored scenarios and pairing.**

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 build/FoloToy-AI-Passport-full.bin
```

After flashing, reconnect Bluetooth and check button input and scenario selection.

## For agents

Start with [AGENTS.md](../../AGENTS.md), follow the
[AI development guide](../development/ai-guide.md), and ensure the required
[Passport skills](../../skills/README.md) are available. Preserve existing changes.

- `main/main.c` and `main/keyboard/`: device interaction, Bluetooth, configuration and storage.
- `main/web/`: hotspot configuration webpage.
- `assets/fonts/` and `tests/`: font assets and validation.

Preserve existing capacity limits, hotspot memory optimizations and stored-data
compatibility. Use the source and tests for implementation contracts.
Run `./tools/validate.sh --static` while iterating and the complete gate before delivery.
For UI changes, check [LVGL previews](../../tests/keypad_preview/) and
[font coverage](../../assets/README.md#passport-keys-fonts).
Report Build, Host tests, Device tests and Unverified separately. Obtain permission
before flashing, and ask the user to perform physical checks that cannot be measured
reliably by the agent. Commit or push only when requested.
