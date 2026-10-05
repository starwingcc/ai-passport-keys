[English](passport-keys.md) · **简体中文**

# Passport Keys

将三键 AI Passport 变成可配置的蓝牙键盘，无需电脑常驻软件。

## 使用

在电脑蓝牙设置中配对 **Passport Keys**。上下键选择**开始控制**、**情景列表**或**设置**，
短按 OK 进入，长按 OK 返回或取消当前动作；进入开始控制后发送按键。

| 按键 | 单击 | 双击 | 三击 | 长按 |
| --- | --- | --- | --- | --- |
| 上 | Esc | 无 | 无 | 保持左 Option |
| 下 | Backspace | 左 Command+A | 无 | 保持方向下 |
| OK | 回车 | 无 | 无 | 返回／取消 |

设置顺序为**屏幕 → 配对系统 → 热点配置 → 清除蓝牙配对**。

- **屏幕**：调整亮度及空闲变暗／息屏计时，0 表示关闭计时。
  上下键调整，短按 OK 保存，长按 OK 取消；息屏后首次按键只唤醒，松开后再操作。
- **配对系统**：手动选择全局 macOS（首次默认）或 Windows。
  上下键只预览，OK 保存，长按 OK 返回，`*` 标记已保存的系统。
  保存失败保留原值；切换系统会取消动作并释放按键。
- **清除蓝牙配对**：换电脑时清除配对，并在旧电脑上忽略 Passport Keys。
  快捷键、宏、情景及所选系统在清除配对和重启后仍保留。

## 网页配置

1. 进入**设置 → 热点配置**并开启，蓝牙暂时断开。
2. 使用屏幕显示的名称和密码连接热点，打开 `http://192.168.4.1`。
3. 编辑情景、手势及键盘／媒体步骤，步骤为空即禁用动作；**设为当前**选择当前情景。
   保存后在设备长按 OK，或点击**退出热点**恢复蓝牙。

左右 **Cmd/Ctrl** 在 macOS 发送对应的 Command，在 Windows 发送对应的 Ctrl，
支持宏步骤及与 Shift 等按键组合。固定修饰键和录制按键保持原意，默认 Command+A 仍是固定键。

**工牌首页**设置身份、头像、UTC 时差及 12／24 小时制，名称和短标签最多 16 个支持的字符。
**同步手机／电脑时间**手动校时；自动校时需选择 2.4 GHz Wi-Fi，测试后再保存，测试不会保存。
刷新后密码留空可保留已保存密码；取得有效时间前首页显示 `--:--`。

导入 JSON 先加载编辑区，保存后生效；导出仅备份情景。
情景导入／导出和恢复默认不包含工牌、屏幕、配对系统、Wi-Fi 或蓝牙配对设置。

## 构建与刷写

按[环境指南](../development/engineering/environment-setup.zh_CN.md)激活 **ESP-IDF 5.5.3**。
设备开机并用 USB 数据线连接后，在仓库根目录执行：

```sh
./tools/validate.sh
python -m serial.tools.list_ports
PASSPORT_PORT=/dev/cu.usbmodem1201  # 替换为实际检测到的端口。
python -m esptool \
  --chip esp32c3 --port "$PASSPORT_PORT" --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 8MB \
  0x10000 build/FoloToy-AI-Passport.bin
```

现有分区布局兼容时，仅更新应用可保留 NVS。
首次安装或有意完整刷新时，将末行替换为 `0x0 build/FoloToy-AI-Passport-full.bin`，
**这会重置已保存设置及配对**。参见[刷写与数据政策](../development/engineering/firmware-layout.zh_CN.md#烧录与已存数据)。

## Agent 开发入口

先读 [AGENTS.zh_CN.md](../../AGENTS.zh_CN.md)、[AI 开发指南](../development/ai-guide.zh_CN.md)
和必需的 [Passport 技能](../../skills/README.zh_CN.md)。
应用代码在 `main/main.c`、`main/keyboard/`，网页在 `main/web/`；板级逻辑复用 `components/bsp`。
保留存储兼容性，设备文字遵循[字体指南](../../assets/README.zh_CN.md#passport-keys-字库)。
主机测试及 [LVGL 预览](../../tests/keypad_preview/)位于 `tests/`；修改期间运行静态门禁，交付前运行完整门禁。
