[English](passport-keys.md) · **简体中文**

# Passport Keys 应用说明

Passport Keys 将三键 AI Passport 变成支持动作和情景配置的蓝牙键盘，无需电脑常驻软件。

## 使用

像素工牌首页显示身份、时间、电量、蓝牙状态，以及 **SCENE** 下的当前情景。
上下键选择**开始控制**、**情景列表**或**设置**，短按 OK 进入，长按 OK 返回。
在电脑蓝牙设置中连接 **Passport Keys**，进入开始控制后即可发送指令。
通过情景列表切换情景。

默认动作：

| 按键 | 单击 | 双击 | 三击 | 长按 |
| --- | --- | --- | --- | --- |
| 上 | Esc | 无 | 无 | 保持左 Option |
| 下 | Backspace | 左 Command+A | 无 | 保持方向下 |
| OK | 回车 | 无 | 无 | 菜单／返回／取消 |

长按 OK 可取消动作。换电脑时，在**设置 → 清除蓝牙配对**中清除配对，并在旧电脑上忽略设备。
这不会删除情景。

**设置 → 屏幕**可调整正常／变暗亮度及空闲计时：变暗 0～180 秒、息屏 0～300 秒，
步长 10 秒，0 表示关闭；同时启用时，变暗须早于息屏。
亮度实时预览，上下键调整，短按 OK 保存，长按 OK 取消；设置重启后保留。
息屏后首次按上、下或 OK 键只唤醒，松开后再操作。

配置设备：

1. 进入**设置 → 热点配置**，选择**开启**并确认。蓝牙暂时断开。
2. 按屏幕显示的名称和密码连接热点，在浏览器打开 `http://192.168.4.1`。
3. 编辑情景、按键手势和键盘／媒体步骤；步骤为空即禁用动作。**设为当前**选择当前情景，然后保存修改。
4. 在设备长按 OK，或在网页点击**退出热点**。蓝牙恢复。

**工牌首页**可设置用户名称、可留空的短标签、机器人／猫／宇航员／芯片头像、固定 UTC 时差和
12／24 小时制。名称和短标签最多 16 个支持的字符，长文字缩小或省略。
默认 UTC+8、24 小时制；夏令时变化时需手动调整时差。

**同步手机／电脑时间**使用浏览器所在设备的系统时间校时。
如需开机自动校时，点击**扫描附近 Wi-Fi**选择 2.4 GHz 网络，隐藏网络可手动输入。
填写密码后点击**测试连接与校时**，结果区分认证失败、未找到网络、超时和校时失败；
即使校时失败，认证也可能已通过。测试时保持浏览器连接 Passport 热点，网页会重试短暂失联。
测试不会自动保存，之后需点击**保存自动校时网络**。
密码默认显示，通过**显示密码**切换显隐。保存后保留本次填写的密码；刷新页面后，
密码框留空即可保持已保存的密码。**清除已保存网络**移除网络设置。
网络可用时，设备在退出热点后及开机时自动校时；取得有效时间前首页显示 `--:--`。

**导出当前情景**分享选中的情景，**导出全部配置**备份所有情景。
**导入 JSON**先加载编辑区，检查后保存才写入设备。
情景导入／导出和恢复默认不包含工牌设置、屏幕设置、Wi-Fi 凭据或蓝牙配对。

## 构建与刷写

使用 **ESP-IDF 5.5.3**，在仓库根目录执行；需要时先按
[环境指南](../development/engineering/environment-setup.zh_CN.md)准备。
以下使用 `build/tooling` 下的安装，其他安装位置请调整路径：

```sh
export IDF_TOOLS_PATH="$PWD/build/tooling/espressif"
export IDF_PYTHON_ENV_PATH="$IDF_TOOLS_PATH/python_env/idf5.5_py3.12_env"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
source "$PWD/build/tooling/esp-idf-v5.5.3/export.sh"
PASSPORT_PY="$IDF_PYTHON_ENV_PATH/bin/python"
./tools/validate.sh
```

开机后用 USB 数据线连接设备。在同一个终端列出串口，将示例端口换成实际设备端口：

```sh
"$PASSPORT_PY" -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201
```

**保留设置和配对的更新**：现有分区布局不变时，仅将已校验的应用镜像写入 **`0x10000`**：

```sh
"$PASSPORT_PY" -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

**首次安装或有意完整刷新**：使用同一命令，将末行换为
`0x0 build/FoloToy-AI-Passport-full.bin`。**这会重置已保存的工牌、情景、网络和配对数据。**

## Agent 开发入口

从 [AGENTS.zh_CN.md](../../AGENTS.zh_CN.md)、[AI 开发指南](../development/ai-guide.zh_CN.md)
及必需的 [Passport Skill](../../skills/README.zh_CN.md) 开始，保留已有改动。

- `main/main.c`、`main/keyboard/`：应用 UI、交互、蓝牙和配置。
- `main/web/`：热点配置网页。
- `assets/fonts/`、`tests/`：字库、主机测试和 [LVGL 预览](../../tests/keypad_preview/)。

可复用的硬件逻辑放入 BSP。保留存储数据兼容，文字修改检查
[字库覆盖](../../assets/README.zh_CN.md#passport-keys-字库)。
迭代时运行 `./tools/validate.sh --static`，交付前运行完整门禁。
分别报告 Build、Host tests、Device tests 和 Unverified。刷写前取得授权，仅在用户要求时提交或推送。
