[English](passport-keys.md) · **简体中文**

# Passport Keys

Passport Keys 将三键 ESP32-C3 Passport 变成蓝牙键盘，启动进入独立中文指令控制台，
无需安装电脑常驻软件。目标为 ESP-IDF 5.5.3、8MB Flash、无 PSRAM、240×320 屏幕。

## 快速开始：使用设备

1. 按下文刷写说明安装已验证的 Passport Keys 固件。开机默认进入指令控制台。
2. 在电脑蓝牙设置连接 **Passport Keys**。采用 Just Works 配对，无需六位配对码，
   也无需在 Passport 上确认数字。
3. 等待控制台显示蓝牙已连接，打开文本编辑器。上／下移动光标，短按 OK 发送回车。
4. 长按 OK **500ms** 打开主菜单，进入设置，可通过热点修改映射或清除蓝牙配对。

如果电脑缓存了旧固件的其他设备名称，在 Passport 清除配对，并在电脑忽略这台键盘，
然后重新连接。清除蓝牙配对不会删除按键映射。

## 按键与导航

```text
指令控制台（默认开机页）
└── 长按 OK → 主菜单
    ├── 指令模式 → 指令控制台
    └── 设置
        ├── 热点配置 → 热点名称、密码、网页地址
        └── 清除蓝牙配对 → 取消／确认
```

上／下选择，短按 OK 进入或确认。长按 OK 返回上一层，在主菜单不执行操作。
只有指令控制台发送电脑按键。OK 长按全局保留，不允许映射。

| 实体键 | 单击 | 双击 | 长按 |
| --- | --- | --- | --- |
| 上 | 方向上 | 禁用 | 保持方向上 |
| 下 | 方向下 | 禁用 | 保持方向下 |
| OK | 回车 | 禁用 | 固定返回 |

默认共有八个可配置动作。单击松开后等待 **180ms** 双击窗口。上／下长按达到
**500ms** 后按下映射，实体键松开时释放，重复速度由电脑决定。双击的第二次按下
若保持到长按阈值，转为长按，不发送双击。

页面和连接切换会取消待触发手势、释放报告，并等待实体键松开。过期输入直接丢弃，
不补发。ADC 电阻梯只能识别一个实体键；电脑组合键应配置到一个动作，而非同时
按多个实体键。60 秒无实体按键输入时，背光降至 12%，再次按键恢复 80%。

## 通过 Wi-Fi 配置映射

1. 长按 OK，进入设置，再选择热点配置。
2. 连接屏幕显示的 `Passport-Keys-XXXX`，输入屏幕密码。每次进入生成新的 12 位
   WPA2 密码，只允许一个客户端。
3. 打开 `http://192.168.4.1`。HTML、CSS 和 JavaScript 全部内置于固件。
4. 选择上／下／OK，修改单击、双击或长按动作。可以录制组合键，也可手动选择
   普通键和左右修饰键。OK 长按始终只读。
5. 保存后映射写入设备。保存失败时继续使用设备原配置。
6. 在 Passport 长按 OK 或在网页点击退出热点。设备关闭网页和 Wi-Fi、恢复 BLE，
   不重启，返回设置。返回主菜单并进入指令模式，即可使用保存后的映射。

热点期间暂停 BLE。网页也可以请求清除蓝牙配对：先退出热点，再在 Passport
显示确认页，必须在设备上确认才会删除配对记录。

录制使用 `KeyboardEvent.code` 识别物理位置，忽略自动重复；取消、失焦或页面隐藏
会清空录制。系统快捷键、输入法、Fn 和浏览器保留组合可能被拦截，无法录制时使用
手动选择。未保存的编辑不会影响设备。

## 支持范围与配对

- 标准键盘用法码 `0x04–0x73`，排除电源键 `0x66`，包含 F1–F24 和小键盘。
- 左右 Control、Shift、Option/Alt、Command/GUI，共八个修饰位。一个组合键最多
  六个不同普通键加修饰键，也允许只配置修饰键。
- 媒体控制：音量加减、静音、播放暂停、上一首（`0xB6`）、下一首（`0xB5`）。
  媒体输入依赖独立通知订阅；Boot 协议只发送键盘报告。

不包含真正的 Apple Fn／地球键、厂商专用键、亮度／背光／电源控制、宏、文本输入、
多套配置或多电脑切换。标准用法码见
[USB HID 用法表](https://www.usb.org/sites/default/files/hut1_21_0.pdf)。

设备保存一台电脑的配对记录，断开后重新广播。换电脑前，在设置清除配对，并在电脑
忽略旧记录。Just Works 保留加密和配对记录，但首次配对不通过数字核对提供 MITM
身份认证；参见[蓝牙安全规范](https://www.bluetooth.com/wp-content/uploads/Files/Specification/HTML/Core-62/out/en/host/security-manager-specification.html)。
发送报告要求已连接、已加密、已配对、16 字节密钥及键盘通知订阅，有意不要求表示
MITM 认证的 `authenticated` 标志。旧数字确认界面已从应用中移除。

## 快速开始：开发

先读根 [AGENTS.zh_CN.md](../../AGENTS.zh_CN.md) 和
[AI 开发指南](../development/ai-guide.zh_CN.md)，核对五个必需的
[Passport Skill](../../skills/README.zh_CN.md)，只使用当前阶段对应的 Skill。
继续当前应用分支，保留本地修改。使用仓库内 Skill 链接的客户端，首次可执行
`python3 tools/install_passport_skills.py`；其他客户端按 Skill 指南选择安装方法。
纯人工开发无需安装 AI 客户端的 Skill。

按[环境指南](../development/engineering/environment-setup.zh_CN.md)安装并激活
**ESP-IDF 5.5.3**。主机测试需要 C 编译器、Python 3、Node.js 和 actionlint；
验证脚本会在缺少 actionlint 时安装校验过的版本。可用
`NODE=/absolute/path/to/node` 指定本地 Node。以下命令从仓库根目录执行：

```sh
source /path/to/esp-idf-v5.5.3/export.sh
python3 tools/install_passport_skills.py --check
./tools/validate.sh --static
./tools/validate.sh
```

完整门禁从 `sdkconfig.defaults` 生成独立配置，运行主机测试、构建并验证合并镜像，
输出 `build/FoloToy-AI-Passport-full.bin`，在
`build/firmware/<full-image-sha256>/` 保留匹配的应用、bootloader、分区表、ELF、
MAP 和 `manifest.json`。刷写或解析崩溃前，校验选定的归档：

```sh
python3 tools/archive_firmware.py verify build/firmware/<full-image-sha256>
```

不提交生成的固件、原始日志、本机路径和临时交接记录。被忽略的 `build/` 用于本地
构建和调试输出。仓库原有硬件测试 demo 保留为参考，不编译进 Passport Keys。

### 代码地图与安全扩展点

| 文件或目录 | 职责 |
| --- | --- |
| `main/main.c` | 初始化、输入队列、应用工作任务、按键释放、蓝牙／热点切换和背光 |
| `main/keyboard/keypad_model.*` | 不依赖 ESP-IDF／LVGL 的导航、手势时序、配置校验和 HID 报告编码 |
| `main/keyboard/keypad_config.*` | 严格 JSON 解析和序列化 |
| `main/keyboard/keypad_store.*` | 互斥保护的当前配置及 NVS 提交 |
| `main/keyboard/keypad_ble.*` | HID 服务、GATT 句柄发现、安全检查、订阅及串行主机命令 |
| `main/keyboard/keypad_web.*`、`main/web/` | 有界热点／HTTP 生命周期和映射编辑器 |
| `main/keyboard/keypad_ui.*` | 五个中文 LVGL 页面、字体及可见状态缓存 |
| `assets/fonts/`、`tools/generate_keypad_fonts.py` | 授权字体子集、生成的 LVGL 字库和字形清单 |
| `tests/test_keypad_*`、`tests/keypad_preview/` | 主机回归、平台桩及实际 LVGL 预览 |
| `components/bsp/` | 可复用板级驱动；新增松开事件位于枚举末尾，保留旧枚举值 |

扩展应用时保留以下约束：

- 按键回调只更新实体键状态并将带时间戳的事件入队。应用工作任务处理 BLE 命令、
  网络和存储。队列容量为 24；溢出或输入过期 250ms 时取消手势并同步实体键状态。
- NimBLE 串行处理 GAP／GATT 和报告发送。应用工作任务是 BLE 命令 API 的唯一
  调用者。主机命令超时后仍持有参数，直到完成，不能提前复用共享缓冲区。
- 不硬编码 GATT 句柄；通过注册回调发现，保留 SDK 的注册／同步／重置回调。
  广播 UUID 必须保持静态生命周期以支持 IDF 重试。控制器已占用连接、CONNECT
  回调尚未到达时，`MAX_CONNECTIONS=1` 不能再次启动广播。
- ESP-IDF 5.5.3 导出的 `ble_gap_conn_foreach_handle` 缺少公共原型，本地声明匹配
  该固定版本。其 HID 等级 3 的 Boot 标志存在语法缺陷，因此使用等级 2 加密
  GATT，不修改 SDK。升级 IDF 时重新核对这两项。
- 应用访问 LVGL 必须持有 `bsp_lvgl_lock()`。屏幕只创建一次并复用，可见状态缓存
  不包含手势时间戳。
- 可测试行为放在模型层并添加主机测试。扩展支持的按键时，同步更新 C 和 JavaScript
  的键码目录，始终保留 OK 长按。
- 修改设备文字必须重新生成**两个字号**并验证覆盖，参见
  [字体资源](../../assets/README.zh_CN.md#passport-keys-字库)和
  [中文字体检查表](../development/engineering/coding-conventions.zh_CN.md#中文字体与缺字)。
- 持久结构和严格 JSON 契约均有版本；增加字段或改变布局需要明确迁移，保留
  无效／不支持配置回退以及 NVS 写入失败时不替换当前配置的行为。

### 配置与 HTTP 契约

主机 JSON 测试原样复制了 [cJSON](https://github.com/DaveGamble/cJSON/tree/c859b25da02955fef659d658b8f324b5cde87be3)，
固定提交为 `c859b25da02955fef659d658b8f324b5cde87be3`，与 ESP-IDF 5.5.3 一致。
MIT 许可保存在 [tests/third_party/cjson/LICENSE.txt](../../tests/third_party/cjson/LICENSE.txt)。
固件使用 IDF 的 JSON 组件，不编译主机测试副本。字体来源和 SIL Open Font License
记录在前述字体资源指南中。

版本 1 仅含 `version: 1` 和八个 `bindings`，依次为上单击／双击／长按、
下单击／双击／长按、OK 单击／双击。每项含 `type`（0 禁用、1 键盘、2 媒体）、
`modifiers`（0–255）、`keys`（最多六个不同、合法、非零键码）和 `consumer`
（键盘／禁用时为零）。拒绝未知／重复字段、第九项、不支持的版本和非法组合。
映射存于 NVS 命名空间 `passport_keys`、blob `config_v1`，BLE 配对单独存储。
缺失或损坏时回退默认配置，不通过启动时擦除 NVS 掩盖错误。

| 请求 | 结果 |
| --- | --- |
| `GET /api/config` | 当前热点会话的 `{config, session}` |
| `POST /api/config` | 校验并保存，仅 NVS 提交成功后报告成功 |
| `POST /api/exit` | 空请求体；退出热点，返回设置 |
| `POST /api/forget` | 空请求体；退出热点，在设备上请求确认 |

POST 必须带 GET 返回的 `X-Session`。API Host 必须为 `192.168.4.1`，可附加
`:80`，不开放 CORS。请求体最多 2048 字节，HTTP socket 最多三个，Wi-Fi 客户端
最多一个。先停止 HTTP，再释放 Wi-Fi／netif；退出热点后将配置重新载入模型。

## 验证、预览与刷写

静态门禁覆盖手势边界／计数回绕、八映射、导航和输入取消、报告编码、严格 JSON、
NVS 失败恢复、GATT 注册／订阅、Just Works 安全检查、BLE 暂停／超时／分配失败、
网页录制及生成字库的 cmap。平台桩不能证明射频或操作系统兼容性。

IDF 拉取 Managed Components 后，可以渲染五个实际 LVGL 页面：

```sh
cmake -S tests/keypad_preview -B build/preview
cmake --build build/preview
mkdir -p build/preview/screens
build/preview/keypad_preview build/preview/screens
```

从仓库根目录执行。预览验证真实字形查找、缺字负例和界面缓存，输出五张 PPM。
需要 CMake 和固定版本的实际 LVGL 依赖；不需要串口，也不会控制电脑界面。

空设备或有意完整刷新时，将校验过的合并 `full.bin` 写入 **`0x0`**。它填充 NVS
间隙，可能重置映射和配对。要保留配置，先核对设备现有分区表、应用偏移和容量，
再将匹配应用写入实际应用起点，仓库默认是 **`0x10000`**。不能将应用单镜像写入
`0x0`。不要求原固件备份或整片擦除。遵循
[刷写与数据政策](../development/engineering/firmware-layout.zh_CN.md#刷写与存储数据)，
写入设备前取得授权。

激活 IDF 后枚举串口，将 `PORT` 替换为选定的 Passport 端口。经授权的完整刷新：

```sh
python -m serial.tools.list_ports -v
python -m esptool --chip esp32c3 --port PORT write_flash --flash_size 8MB \
  0x0 build/FoloToy-AI-Passport-full.bin
```

经授权的保留配置更新，则使用校验过的归档内 `FoloToy-AI-Passport.bin`，写入已核对
的应用偏移。不打开任意蓝牙／串口端口，也不使用构建失败后遗留的旧镜像。

用户已在 Mac 验证基本上／下／OK 输入和首次免输码配对。后续源码修改仍需自己的
设备回归。检查长按释放、媒体键、断开／重连不补发、全部返回路径、十轮热点进出、
重启后映射持久化、实屏中文及运行时堆／栈稳定性。分别报告 Build、Host tests、
Device tests 和 Unverified。

### 排查入口

| 现象 | 优先检查 |
| --- | --- |
| 电脑显示连接但按键被忽略 | 设备连接／加密／句柄／订阅状态；只有指令控制台发送 |
| 旧设备名或配对反复失败 | 在 Passport 清除配对，电脑忽略同一键盘后重新配对 |
| 等待电脑启用键盘 | 通知订阅、发现的句柄和 Boot／report 协议 |
| 媒体键不可用 | Consumer 通知订阅；Boot 协议不支持媒体键 |
| 广播错误 6 | 先查 NimBLE 连接名额及延迟的 CONNECT，再查实际分配证据；不能直接等同于系统堆耗尽 |
| 无法打开网页 | 连接屏幕显示的热点并使用 `http://192.168.4.1`；检查 USB 热点启动日志 |
| 新增中文缺字 | 重生成两个字号，验证 cmap／控件字体绑定并检查实屏 |

USB 日志保留同步／句柄、连接／安全状态、HID ready 变化、断开、忽略输入和内存
快照。订阅和报告细节使用 DEBUG 日志，需要时在本地诊断构建启用该级别。
不记录配对码、加密密钥和报告内容。解析地址前，将启动 ELF 标识匹配到校验过的
归档，原始日志只留在被忽略的本地输出目录。
