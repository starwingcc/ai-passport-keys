[English](passport-keys.md) · **简体中文**

# Passport Keys 应用说明

Passport Keys 将三键 AI Passport 变成支持动作和情景配置的蓝牙键盘，无需电脑常驻软件。

## 使用

开机进入**开始控制**。在电脑蓝牙设置中连接 **Passport Keys**，即可通过设备按键发送指令。

默认映射：

| 按键 | 单击 | 双击 | 长按 |
| --- | --- | --- | --- |
| 上 | Esc | 禁用 | 保持左 Option |
| 下 | Backspace | 左 Command+A | 保持方向下 |
| OK | 回车 | 禁用 | 打开菜单／返回 |

菜单中，上下选择，短按 OK 进入或确认，长按 OK 返回。通过**情景列表**切换当前情景。
换电脑时，在**设置 → 清除蓝牙配对**中清除配对，并在旧电脑上忽略设备。清除配对不会删除情景。

配置动作：

1. 进入**设置 → 热点配置**，选择**开启**并确认。蓝牙暂时断开；确认页默认选中取消。
2. 按屏幕显示的名称和密码连接热点，在浏览器打开 `http://192.168.4.1`。
3. 新增或编辑情景，选择实体键和手势，填写标题并添加键盘／媒体步骤。步骤列表为空即禁用。
   如需更改开机情景，点击**设为当前**，然后保存修改。
4. 在设备长按 OK，或在网页点击**退出热点**。蓝牙恢复，返回开始控制即可使用已保存的动作。

最多支持 **8 个情景**、**每动作 64 步**，情景名称和动作标题最多 **16 个字符**，
网页显示总容量。OK 长按保留给导航和取消。单步长按保持到松开，多步长按只执行一次。
动作执行期间忽略新指令，长按 OK 可取消。

## 构建与刷写

使用 **ESP-IDF 5.5.3**；未安装时先按[环境指南](../development/engineering/environment-setup.zh_CN.md)
准备环境。在仓库根目录执行。以下使用当前 `build/tooling` 下的已有安装；
其他安装位置请调整路径：

```sh
export IDF_TOOLS_PATH="$PWD/build/tooling/espressif"
export IDF_PYTHON_ENV_PATH="$IDF_TOOLS_PATH/python_env/idf5.5_py3.12_env"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
source "$PWD/build/tooling/esp-idf-v5.5.3/export.sh"
PASSPORT_PY="$IDF_PYTHON_ENV_PATH/bin/python"
./tools/validate.sh
```

完整门禁会测试、构建并校验固件，生成 `build/FoloToy-AI-Passport.bin` 和
`build/FoloToy-AI-Passport-full.bin`。开机后用 USB 数据线连接设备，查看串口并设置
`PASSPORT_PORT`。当前 macOS 设备使用 `/dev/cu.usbmodem1201`；如果列出的端口不同，
请使用实际值：

```sh
"$PASSPORT_PY" -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201
```

刷写命令需在同一个终端执行；新开终端时，先重新激活 ESP-IDF 并设置 `PASSPORT_PY`。

### 更新并保留情景和配对

本项目现有分区布局不变时，仅将应用镜像写入 **`0x10000`**：

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

### 首次安装或完整刷新

将合并镜像写入 **`0x0`**。**这会清除已保存的情景和配对。**

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x0 build/FoloToy-AI-Passport-full.bin
```

刷写后重新连接蓝牙，检查按键输入和情景切换。

## Agent 开发入口

从 [AGENTS.zh_CN.md](../../AGENTS.zh_CN.md) 开始，遵循
[AI 开发指南](../development/ai-guide.zh_CN.md)，确认必需的
[Passport Skill](../../skills/README.zh_CN.md) 可用，并保留已有改动。

- `main/main.c` 和 `main/keyboard/`：设备交互、蓝牙、配置和存储。
- `main/web/`：热点配置网页。
- `assets/fonts/` 和 `tests/`：字库资源与验证。

保留既有容量上限、热点内存优化和存储数据兼容，具体实现约束以源码和测试为准。
迭代时运行 `./tools/validate.sh --static`，交付前运行完整门禁。界面修改还需检查
[LVGL 预览](../../tests/keypad_preview/)和[字体覆盖](../../assets/README.zh_CN.md#passport-keys-字库)。
分别报告 Build、Host tests、Device tests 和 Unverified。刷写前取得用户授权，
难以由 Agent 可靠测量的实机项目交给用户操作。未经请求不提交、不推送。
