[简体中文](passport-transfer-plan.zh_CN.md) · **English**

# Passport browser transfer: next development increment

Status: development proposal, researched on 2026-10-06. HTTPS hosting and the
large-file workflow below are not implemented. Start with [AGENTS.md](../../AGENTS.md)
and the [application guide](passport-keys.md); preserve the existing transfer
implementation and its tests.

## Accepted requirements

- Bidirectional transfer of ordinary user-selected movie files between PC and
  Android, with browsers only and no installed PC helper. Android is the first
  mobile target; iPhone is outside the first acceptance commitment.
- Internet access to load an auxiliary/transfer webpage is acceptable. File
  content must stay on the LAN. Do not upload content to a website or add a
  public TURN relay. Keep filenames and hashes out of website requests/analytics.
- Passport provides discovery and signaling. It must not cache or relay shared
  text/file payloads, acquire a filesystem, or change Flash partitions for transfer.
- The user uses Via on Android. Treat Via as a compatibility target to investigate,
  not as equivalent to Chrome or as already supported.
- The current increment raises the mobile memory receive budget to **64 MiB**
  (67,108,864 bytes). Desktop remains 128 MiB. This is a per-page aggregate of
  pending receives and retained downloads, not a separate allowance per file.

## Current implementation and unresolved acceptance

| Area | Current behavior |
| --- | --- |
| Entry | Device-displayed HTTP address; `/` redirects to `/transfer` |
| Sessions | Four browser sessions; tabs count separately |
| Resources | Six HTTP/WS client sockets, one pending accept, ten lwIP sockets in total; HTTPD also uses internal sockets |
| Protocol | Eight shared files per sender, 8 GiB per-file metadata ceiling, 8,000 UTF-8 bytes per text message |
| Data | WebRTC DataChannel with no ICE servers; 16 KiB maximum chunks, 256 KiB send window, incremental SHA-256 |
| Receive | Secure-context file picker uses a writable file; ordinary private-IP HTTP uses a bounded Blob download |
| Concurrency | At most two outgoing file transfers per page; one incoming/outgoing file per peer link |
| Lifecycle | 120-second heartbeats, eight-second signaling connection deadline, five automatic reconnect attempts |
| Interruption | No resume; signaling shutdown currently closes RTC links, refresh removes local shared sources |

The 64 MiB change does not implement large-file mobile saving or resolve the
network stability problem. A later device test completed 20 rounds of resource
loads, an off/on cycle and a normal Mac-to-Windows 100 MiB transfer; the user
confirmed saving, but an independent hash of the Windows saved file was not
checked. Subsequently the user reproduced failure when adding a fourth device
and refreshing pages: multiple clients became inaccessible, recovered, then
failed again. Stability acceptance therefore remains failed. Do not report a
successful build, this partial test, or API availability as full device acceptance.

An earlier same-host browser diagnostic rewrote mDNS ICE candidates through
`KP_RTC_RESOLVE_MDNS=1`. It cannot count as cross-device, mobile, or offline-LAN
acceptance. The old WebRTC-constructor error was an extension blocking the site
and was resolved; it does not explain the remaining shared connectivity failure.

## Recommended architecture

Use one top-level HTTPS transfer page on each endpoint, owning both RTC and file
saving. A static website serves the program; Passport continues local signaling;
file bytes travel directly between the two browsers. Avoid making Android keep
an HTTP transfer tab running behind a separate HTTPS save window.

1. Use a full, current Chrome on PC and Android as the first baseline. Android
   Chrome/WebView gained File System Access in version 132 [1], but the proposed
   direct HTTPS-to-local-WebSocket route uses Chrome 147+ LNA behavior [3, 4].
   Probe actual capabilities and operations; a browser name or version is not
   a successful compatibility test.
2. Load the static HTTPS page and obtain the actual device IP from the device
   entry/pairing flow. Do not hardcode a developer's address. If carrying local
   connection/pairing data in the URL, use a fragment rather than a server-visible
   query, and do not leak it through telemetry.
3. Obtain the browser's local-network permission, then connect to the Passport's
   explicit local IP using WebSocket. Chromium documents a mixed-content
   exception for explicit local IPs with this permission [4]. Do not ask users
   to disable browser security. Prototype this exact path before migrating the UI.
4. Change the current strict HTTP Origin check deliberately: retain Host checks,
   allow only the chosen HTTPS application origin, and design device-local
   pairing authorization. Do not replace it with a wildcard. Hosting/domain and
   pairing UX remain implementation decisions; publishing needs authorization.
5. On receive, open the save picker from a user action before starting the data
   transfer. Create the writable stream, write ordered chunks, hash incrementally,
   and acknowledge only after each write resolves. Bound every queued stage,
   including the RTC receive promise chain and browser send buffer.
6. Report success only after final hash validation and successful stream close.
   Handle picker cancellation, permission denial, full storage, write/close errors,
   and transport interruption; never silently fall back to a multi-GB Blob.

Native write APIs require a secure context and user interaction [2]. Android's
file picker can select local or cloud-backed providers [6]; acceptance uses a
local Downloads/Documents destination and must not route data through a cloud
provider. Browser/provider temporary files can require extra storage. Streaming
removes the requirement to hold a movie in page memory; it is not an unlimited
size guarantee or automatic resume support.

Via uses Android WebView [7]. Although modern WebView has file-system support,
Chrome's cited LNA WebSocket mechanism explicitly excludes WebView [4]. Verify
Via's actual local connection, picker and writable-file behavior before claiming
support. Retain the bounded small-file fallback where applicable. OPFS staging
and Service Worker downloads are alternative research paths, not the first
Android implementation or evidence of iPhone compatibility; OPFS storage has
quotas and eviction rules [8].

Keep both endpoints foreground and the phone awake for the first accepted
version. Background pages may freeze or be discarded [5]. Offline reopening
after an initial online load requires a separate cache/update design; it is not
guaranteed by ordinary browser caching. Do not promise background transfer,
screen-off operation, simultaneous multi-device movies, or resume in this increment.

## Work sequence and acceptance

1. Build a small capability prototype before changing firmware routing. Verify
   local WebSocket access, user-selected file creation, repeated writes and close
   on actual PC Chrome and Android Chrome; record Android/Via/WebView versions
   separately when checking Via. Do not infer success from method existence alone.
2. Investigate the existing four-client failure independently. Distinguish client
   HTTP/WS occupancy, the accept backlog, TCP queued data/retries, heap's largest
   block, and HTTPD send/queue delays. Preserve the bounded diagnostics; do not
   blindly increase buffers/timeouts or enable global LRU eviction of WebSockets.
   A fifth session should be rejected without disrupting four existing sessions.
3. Integrate the HTTPS entry, restricted signaling access and native writable sink.
   Preserve the 64/128 MiB aggregate memory fallback. Keep the 8 GiB protocol ceiling
   initially; increasing it later requires updating both browser and signaling
   validation, with new acceptance evidence.
4. Review signaling-loss behavior for long transfers. Do not simply remove RTC
   shutdown: explicit device disable must still end sharing. If allowing RTC to
   survive a temporary signaling outage, define reconnection, lease expiry and
   missed-stop behavior before implementing it.
5. First accept one pair and one file at a time, in **both directions**, with
   1 GiB and 4 GiB files. Compare hashes of independently read saved files, verify
   memory does not grow with total bytes, and verify selected RTC candidates and
   network traffic carry no file data over the public Internet. Test cancellation,
   low storage, permission failures, refresh, device off/on and reconnection.
6. Separately accept four foreground sessions, client refresh/arrival/departure,
   sustained idle time, and Bluetooth keyboard operation while Wi-Fi is active.
   Keep a bounded test plan and stop on a reproduced failure; new code or a new
   test plan needs a deliberate next run, not an automatic flash/stress loop.

Run focused host checks and then `./tools/validate.sh` with ESP-IDF 5.5.3 and the
tracked `sdkconfig.defaults`. Report Build, Host tests, Device tests and Unverified
separately. Obtain authorization for the exact new firmware before flashing; the
earlier device-test permission does not authorize future builds or repeated runs.
Keep raw serial logs and matching firmware/ELF archives local, outside commits.
Read-only port discovery must not open/reset the device. If serial observation is
authorized, follow the IDF monitor's DTR/RTS sequence; earlier careless opens reset
the device and invalidated some observations.

## Source map

- `main/web/transfer.js`: receive reservations, native/Blob sinks, RTC, reconnect.
- `main/web/transfer-protocol.js`: sizes, chunking, acknowledgments and SHA-256.
- `main/keyboard/keypad_transfer_signal.c`: bounded metadata and SDP/ICE routing.
- `main/keyboard/keypad_transfer_web.c`: Host/Origin, HTTP/WS slots, polling/stop.
- `main/keyboard/keypad_transfer_io.c`: complete partial writes under a total
  send deadline; failed sockets discard queued data before HTTPD closes them.
- `main/keyboard/keypad_transfer_diag.c`: rate-limited TCP-task snapshots.
- `main/keyboard/keypad_web.c`: shared Flash-backed Content-Length asset responses;
  this is no longer the earlier 512-byte chunked-response implementation.
- `main/keyboard/keypad_network_service.c` and `keypad_network.c`: background
  lifecycle and shared STA/SNTP ownership; keep slow work out of button/LVGL paths.
- `tests/test_transfer_*.c`, `tests/test_transfer_*.js`: transport, lifecycle,
  protocol and browser-session regressions. The optional real-browser harness
  does not replace actual phone, network or firmware acceptance.

## Research sources

1. [Chrome 132: File System Access on Android and WebView](https://developer.chrome.com/release-notes/132#file_system_access_for_android_and_webview).
2. [Chrome: File System Access API](https://developer.chrome.com/docs/capabilities/web-apis/file-system-access).
3. [Chrome 147: local-network WebSocket permission](https://developer.chrome.com/release-notes/147#local_network_access_restrictions_for_websockets).
4. [Chromium launch: mixed-content exceptions and WebView exclusion](https://groups.google.com/a/chromium.org/g/blink-dev/c/O6GMKt44Ups).
5. [Chrome: page lifecycle](https://developer.chrome.com/docs/web-platform/page-lifecycle-api).
6. [Android: local and cloud document providers](https://developer.android.com/training/data-storage/shared/documents-files).
7. [Via for Android FAQ](https://viayoo.com/zh-cn/docs/via-for-android-faq.html).
8. [WebKit: storage quotas and persistence](https://webkit.org/blog/14403/updates-to-storage-policy/).
