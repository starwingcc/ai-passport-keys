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
3. 编辑工牌、Wi-Fi 网络、情景及键盘／媒体步骤，步骤为空即禁用动作；**设为当前**选择当前情景。
   选择实体按键和单击／双击／三击／长按，再用**录制**或**手动选键**编辑键盘步骤。
   **保存全部设置**、**退出热点**及**恢复默认配置**固定在页面顶部。
   点击一次**保存全部设置**保存所有修改，再在设备长按 OK，或点击**退出热点**恢复蓝牙。
   清空 Wi-Fi 也只在保存后生效。三块配置统一保存；保存失败时，设备保留已保存的设置。
   尚未确认保存成功时，请保持页面打开并重试。

左右 **Cmd/Ctrl** 在 macOS 发送对应的 Command，在 Windows 发送对应的 Ctrl，
支持宏步骤及与 Shift 等按键组合。固定修饰键和录制按键保持原意，默认 Command+A 仍是固定键。

**工牌首页**设置身份、头像、UTC 时差及 12／24 小时制，名称和短标签最多 16 个支持的字符。
选择或填写 2.4 GHz Wi-Fi，测试后点击**保存全部设置**，测试不会保存。
重新打开页面会显示已保存的 Wi-Fi 密码，眼睛按钮可隐藏或显示密码；取得有效时间前首页显示 `--:--`。

**导出完整配置**备份工牌、Wi-Fi 和全部按键情景，包含明文 Wi-Fi 密码，请妥善保管文件。
导入 JSON 会将三块配置加载到编辑区，保存后生效；单情景导出只分享按键。
旧版 v2–v5 文件只导入情景，保留当前编辑的工牌与 Wi-Fi。
设备现有设置会自动迁移；新的完整配置文件需要更新后的工作台。

**恢复默认配置**在保存后重置工牌、Wi-Fi 与全部按键情景。
屏幕、配对系统及蓝牙配对设置仍独立于这份配置。

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
