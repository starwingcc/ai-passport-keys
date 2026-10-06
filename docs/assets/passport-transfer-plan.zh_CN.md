[English](passport-transfer-plan.md) · **简体中文**

# Passport 浏览器流转：下一阶段开发提案

状态：开发提案，调研日期为 2026-10-06。下述 HTTPS 托管及大文件流程尚未实现。
从 [AGENTS.md](../../AGENTS.zh_CN.md) 和[应用指南](passport-keys.zh_CN.md)开始，
保留现有流转实现及测试。

## 已确认需求

- PC 与 Android 双向传输用户选择的普通电影文件，使用纯浏览器，不安装 PC 辅助程序。
  Android 为手机首要目标；首批验收不承诺 iPhone。
- 可以联网加载辅助／流转网页，但文件内容必须留在局域网。不得向网站上传内容，
  不增加公网 TURN 中继；文件名、哈希也不得进入网站请求或统计上报。
- Passport 负责发现和信令，不缓存或中继共享文字／文件正文，不增加文件系统，
  不为流转修改 Flash 分区。
- 用户在 Android 使用 Via。将其作为待调查的兼容目标，不视为等同 Chrome 或已支持。
- 本次增量将手机内存接收上限提高到 **64 MiB**（67,108,864 字节），电脑仍为 128 MiB。
  这是每个页面中等待连接的接收任务与尚未释放下载记录的合计额度，不是每个文件独立额度。

## 当前实现与未通过的验收

| 范围 | 当前行为 |
| --- | --- |
| 入口 | 设备显示的 HTTP 地址；`/` 跳转到 `/transfer` |
| 会话 | 四个浏览器会话；每个标签页单独计数 |
| 资源 | 六个 HTTP/WS 客户端 socket，一个待接受连接，lwIP 共十个 socket；HTTPD 还占用内部 socket |
| 协议 | 每个发送端八个共享文件，单文件元数据上限 8 GiB，每条文字最多 8,000 个 UTF-8 字节 |
| 数据 | 无 ICE 服务器的 WebRTC DataChannel；最大 16 KiB 分块、256 KiB 发送窗口、增量 SHA-256 |
| 接收 | 安全上下文中的文件选择器可写文件；普通私网 IP HTTP 使用有容量上限的 Blob 下载 |
| 并发 | 每页最多两个外发文件；每个对端连接各一个接收／发送文件 |
| 生命周期 | 120 秒心跳期限，八秒信令连接期限，最多五次自动重连 |
| 中断 | 不支持续传；信令关闭目前会关闭 RTC；刷新会移除本机共享源 |

64 MiB 修改没有实现手机大文件保存，也没有解决网络稳定性问题。后续一次实机测试完成了
20 轮资源加载、一次关闭重开和正常 Mac→Windows 的 100 MiB 传输；用户确认已保存，
但未独立读取 Windows 保存文件并核验哈希。随后用户在加入第四台设备和刷新网页时再次复现：
多个端无法访问，恢复后又失败。因此稳定性验收仍失败，不能把构建成功、这次局部通过或
浏览器存在某 API 当作完整实机验收通过。

更早的同机双浏览器诊断使用 `KP_RTC_RESOLVE_MDNS=1` 改写 mDNS ICE 候选，
不能作为跨设备、手机或无互联网局域网的验收。早期 WebRTC 构造器错误由扩展拦截站点引起，
已经解决，不是剩余多端共同断线的解释。

## 推荐架构

两端各打开一个顶层 HTTPS 流转页，由同一页面持有 RTC 和保存流程。静态网站提供程序，
Passport 继续局域网信令，文件字节在浏览器间直传。避免让 Android 在前台 HTTPS 保存窗口
背后持续运行另一个 HTTP 传输标签页。

1. 首批以 PC 与 Android 的新版完整 Chrome 为基线。Android Chrome/WebView 从 132 起有
   File System Access [1]，但拟议的 HTTPS→局域网 WebSocket 路径使用 Chrome 147+
   的 LNA 行为 [3, 4]。检测实际能力及操作结果，浏览器名称／版本不等于兼容性验证。
2. 加载静态 HTTPS 网页，从设备入口／配对流程取得实际 IP，不硬编码开发者地址。
   若通过 URL 携带本地连接／配对信息，使用 fragment，不放进服务器可见的 query，
   也不通过统计上报泄漏。
3. 取得浏览器局域网访问权限，再通过明确的本地 IP 用 WebSocket 连接 Passport。
   Chromium 说明该权限下明确私网 IP 可适用混合内容例外 [4]。不要求用户关闭浏览器安全机制，
   必须先验证这条准确路径，再迁移界面。
4. 有针对性地修改当前严格的 HTTP Origin 校验：保留 Host 检查，只允许选定的 HTTPS
   应用来源，并设计设备本地配对授权，不改成通配符。托管域名及配对交互尚待实现时确定；
   发布网站需要授权。
5. 接收时由用户操作先打开保存位置选择器，再启动传输。创建写入流，顺序写分块、增量哈希，
   每块写入完成才发送确认。所有排队环节都要有界，包括 RTC 接收 Promise 队列和浏览器发送缓存。
6. 只有最终哈希校验和写入流关闭均成功后，才报告保存成功。处理选择取消、权限拒绝、磁盘满、
   写入／关闭错误和传输中断，不静默退回多 GB 的 Blob 接收。

原生写入 API 需要安全上下文和用户操作 [2]。Android 文件选择器可能包含本地或云盘提供方 [6]；
验收使用本机下载／文档目录，不能经过云盘。浏览器或文件提供方的临时文件可能额外占用空间。
流式写盘消除了页面容纳整部电影的要求，不等于容量无限，也不自动提供断点续传。

Via 使用 Android WebView [7]。现代 WebView 虽有文件系统能力，但上述 Chrome LNA
WebSocket 机制明确不覆盖 WebView [4]。必须验证 Via 的真实局域网连接、文件选择和持续写入，
才能声明支持；适用时保留小文件容量受限的兼容路径。OPFS 暂存和 Service Worker 下载是备选
研究方向，不作为首批 Android 实现，也不能据此承诺 iPhone；OPFS 有配额和回收规则 [8]。

首版验收要求两端前台、手机不锁屏。后台页面可能被冻结或回收 [5]。首次联网后离线重新打开，
需要单独设计缓存及更新策略，不能靠普通缓存作保证。本阶段不承诺后台、锁屏传输、
多设备同时传电影或断点续传。

## 开发顺序与验收

1. 修改固件路由前先做小型能力原型，在真实 PC Chrome 和 Android Chrome 验证本地 WebSocket、
   用户选择文件、连续写入和关闭。调查 Via 时独立记录 Android、Via、WebView 版本，
   不仅检测方法是否存在。
2. 独立调查现有四端故障，区分 HTTP/WS 客户端占用、accept 队列、TCP 排队数据／重传、
   堆最大连续块及 HTTPD 发送／工作队列延迟。保留有限诊断，不盲目增加缓冲／超时，
   不启用会淘汰 WebSocket 的全局 LRU。第五个会话应被拒绝，而不影响现有四个会话。
3. 集成 HTTPS 入口、受限信令访问和原生写入。保留 64/128 MiB 合计内存兼容上限。
   协议先保留 8 GiB 上限，后续放宽需同时更新浏览器与信令校验，并提供新增验收证据。
4. 评估长传输期间信令中断的行为，不能直接删掉 RTC 关闭逻辑：设备明确关闭共享仍必须生效。
   若允许短暂信令断开时维持 RTC，先定义重连、租约到期和错过停止通知的处理，再实现。
5. 先验收一对设备、一次一个文件，**两个方向**都测试 1 GiB 和 4 GiB。
   独立读取保存文件比较哈希，检查内存不随累计字节增长，并验证选中 RTC 候选与网络流量，
   确保文件数据不经过公网。覆盖取消、空间不足、权限失败、刷新、设备关闭重开及重连。
6. 独立验收四端前台会话、刷新／加入／离开、长期空闲，以及 Wi-Fi 活跃时的蓝牙键盘操作。
   测试计划必须有界，复现失败就停止；代码变化或新测试计划需明确安排下一轮，
   不进行自动反复刷写／压力测试。

先运行相关主机测试，再用 ESP-IDF 5.5.3 和受版本控制的 `sdkconfig.defaults` 运行
`./tools/validate.sh`。分别报告 Build、Host tests、Device tests 和 Unverified。
刷写前须对具体新固件取得授权，早先的实机测试许可不涵盖未来版本或重复测试。
原始串口日志及匹配固件／ELF 归档仅本地保留，不提交。只读端口发现不得打开／复位设备。
若获准串口观察，遵循 IDF monitor 的 DTR/RTS 顺序；早期不当打开曾复位设备，使部分观察无效。

## 源码定位

- `main/web/transfer.js`：接收容量预留、原生／Blob 保存、RTC 和重连。
- `main/web/transfer-protocol.js`：容量、分块、确认和 SHA-256。
- `main/keyboard/keypad_transfer_signal.c`：有界元数据及 SDP/ICE 路由。
- `main/keyboard/keypad_transfer_web.c`：Host/Origin、HTTP/WS 槽位、轮询和停止。
- `main/keyboard/keypad_transfer_io.c`：总发送期限内补全短写；失败 socket 在 HTTPD
  关闭前丢弃排队数据。
- `main/keyboard/keypad_transfer_diag.c`：限频的 TCP 任务快照。
- `main/keyboard/keypad_web.c`：共用的 Flash 资源 Content-Length 响应，
  已不是早期每 512 字节发送一次 HTTP chunk 的实现。
- `main/keyboard/keypad_network_service.c` 与 `keypad_network.c`：后台生命周期及 STA/SNTP
  所有权；慢操作继续远离按键回调和 LVGL 临界区。
- `tests/test_transfer_*.c`、`tests/test_transfer_*.js`：传输、生命周期、协议和浏览器会话回归。
  可选真实浏览器测试不能代替实际手机、网络或固件验收。

## 调研来源

1. [Chrome 132：Android 和 WebView 的文件系统访问](https://developer.chrome.com/release-notes/132#file_system_access_for_android_and_webview)。
2. [Chrome：File System Access API](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access)。
3. [Chrome 147：局域网 WebSocket 权限](https://developer.chrome.com/release-notes/147#local_network_access_restrictions_for_websockets)。
4. [Chromium 发布说明：混合内容例外及 WebView 不适用](https://groups.google.com/a/chromium.org/g/blink-dev/c/O6GMKt44Ups)。
5. [Chrome：页面生命周期](https://developer.chrome.com/docs/web-platform/page-lifecycle-api)。
6. [Android：本地及云端文档提供方](https://developer.android.com/training/data-storage/shared/documents-files)。
7. [Via for Android 常见问题](https://viayoo.com/zh-cn/docs/via-for-android-faq.html)。
8. [WebKit：存储配额及持久性](https://webkit.org/blog/14403/updates-to-storage-policy/)。
