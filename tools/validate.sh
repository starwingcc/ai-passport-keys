#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_demo_navigation.c main/demo_navigation.c \
        -o "${test_dir}/test_demo_navigation"
    "${test_dir}/test_demo_navigation"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_model.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_model"
    "${test_dir}/test_keypad_model"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_screen.c main/keyboard/keypad_screen.c -o "${test_dir}/test_keypad_screen"
    "${test_dir}/test_keypad_screen"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_runner.c main/keyboard/keypad_runner.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_runner"
    "${test_dir}/test_keypad_runner"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard -Itests/third_party/cjson \
        tests/test_keypad_config.c main/keyboard/keypad_config.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c \
        tests/third_party/cjson/cJSON.c -o "${test_dir}/test_keypad_config"
    "${test_dir}/test_keypad_config"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_store_stubs -Imain/keyboard \
        tests/test_keypad_store.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c -o "${test_dir}/test_keypad_store"
    "${test_dir}/test_keypad_store"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_ble_stubs -Imain/keyboard \
        tests/test_keypad_ble.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c main/keyboard/keypad_pairing.c -o "${test_dir}/test_keypad_ble"
    "${test_dir}/test_keypad_ble"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_pairing.c main/keyboard/keypad_pairing.c -o "${test_dir}/test_keypad_pairing"
    "${test_dir}/test_keypad_pairing"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_web_stubs -Itests/keypad_store_stubs -Imain/keyboard \
        tests/test_keypad_http.c main/keyboard/keypad_config.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c \
        -o "${test_dir}/test_keypad_http"
    "${test_dir}/test_keypad_http"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_home.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c main/keyboard/keypad_config.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_home"
    "${test_dir}/test_keypad_home"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_network_stubs -Itests/keypad_web_stubs -Itests/keypad_store_stubs -Imain/keyboard \
        tests/test_keypad_network.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_network"
    "${test_dir}/test_keypad_network"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard \
        tests/test_keypad_wifi.c main/keyboard/keypad_config.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_wifi_setup.c main/keyboard/keypad_model.c main/keyboard/keypad_devices.c main/keyboard/keypad_screen.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_wifi"
    "${test_dir}/test_keypad_wifi"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_network_stubs -Itests/keypad_web_stubs -Itests/keypad_store_stubs -Imain/keyboard \
        tests/test_keypad_wifi_setup_runtime.c main/keyboard/keypad_wifi_setup.c main/keyboard/keypad_home.c main/keyboard/keypad_wifi.c main/keyboard/keypad_text.c \
        -o "${test_dir}/test_keypad_wifi_setup_runtime"
    "${test_dir}/test_keypad_wifi_setup_runtime"
    "${test_dir}/test_keypad_config" --defaults > "${test_dir}/keypad-defaults.json"
    KP_DEFAULTS_JSON="${test_dir}/keypad-defaults.json" KP_CONFIG_TEST="${test_dir}/test_keypad_config" "${NODE:-node}" tests/test_keypad_web.js
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/keyboard -Itests/third_party/cjson \
        tests/test_transfer_signal.c main/keyboard/keypad_transfer_signal.c tests/third_party/cjson/cJSON.c -lm -o "${test_dir}/test_transfer_signal"
    "${test_dir}/test_transfer_signal"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/keypad_web_stubs -Imain/keyboard \
        tests/test_socket_io.c -o "${test_dir}/test_socket_io"
    "${test_dir}/test_socket_io"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/transfer_stubs -Itests/keypad_web_stubs -Itests/third_party/cjson -Imain/keyboard \
        tests/test_transfer_http.c main/keyboard/keypad_transfer_signal.c tests/third_party/cjson/cJSON.c -lm -o "${test_dir}/test_transfer_http"
    "${test_dir}/test_transfer_http"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Itests/transfer_stubs -Itests/keypad_web_stubs -Itests/keypad_store_stubs -Imain/keyboard \
        tests/test_transfer_lifecycle.c main/keyboard/keypad_wifi.c main/keyboard/keypad_text.c -o "${test_dir}/test_transfer_lifecycle"
    "${test_dir}/test_transfer_lifecycle"
    "${NODE:-node}" tests/test_transfer_protocol.js
    "${NODE:-node}" tests/test_transfer_session.js
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_keypad_fonts.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_display_rounding.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_display_rounding"
    "${test_dir}/test_bsp_display_rounding"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Icomponents/bsp/src \
        tests/test_bsp_es8311_sleep_check.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_es8311_sleep_check"
    "${test_dir}/test_bsp_es8311_sleep_check"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_button.c -o "${test_dir}/test_bsp_button"
    "${test_dir}/test_bsp_button"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/bsp_stubs -Icomponents/bsp/include \
        tests/test_bsp_lvgl_init.c components/bsp/src/bsp_display_rounding.c \
        -o "${test_dir}/test_bsp_lvgl_init"
    "${test_dir}/test_bsp_lvgl_init"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Itests/audio_stubs -Icomponents/bsp/include -Icomponents/bsp/src \
        tests/test_bsp_audio_recovery.c components/bsp/src/bsp_es8311_sleep_check.c \
        -o "${test_dir}/test_bsp_audio_recovery"
    "${test_dir}/test_bsp_audio_recovery"
    local dead_strip_flag="-Wl,--gc-sections"
    if [[ "$(uname -s)" == "Darwin" ]]; then
        dead_strip_flag="-Wl,-dead_strip"
    fi
    for demo in audio low_power ble wifi; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
            -ffunction-sections -fdata-sections -Itests/demo_stubs -Imain \
            "tests/test_demo_${demo}_runtime.c" "${dead_strip_flag}" \
            -o "${test_dir}/test_demo_${demo}_runtime"
        "${test_dir}/test_demo_${demo}_runtime"
    done
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_deep_sleep_contract.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_check_repo.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_verify_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_archive_firmware.py
    PYTHONDONTWRITEBYTECODE=1 python3 tests/test_install_passport_skills.py
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    PYTHONDONTWRITEBYTECODE=1 python3 tools/archive_firmware.py create \
        "${validation_build_dir}" --archive-root "${repo_root}/build/firmware"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport.bin" \
        "${repo_root}/build/FoloToy-AI-Passport.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
