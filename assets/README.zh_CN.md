<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 资源目录（Assets）

本目录集中存放可复用的资源（字库、图片、音乐等），按资源类型分子目录管理。每个资源放在其类型对应的子目录，并记录放置路径、命名方式、集成方式与来源/许可。二进制资源（字体、图片、音频）不属于纯 markdown 文档，请勿与文档混放。涉及版权/授权的资源需注明来源与许可。

## 字库（fonts）

可复用的字库文件与生成的字库源码放在 `fonts/`。

- 命名要能反映字族、字重、字级与格式。
- 记录来源、许可、字符范围、转换命令与目标放置路径。
- 添加字库前评估 Flash 与内部 RAM 影响；ESP32-C3 无 PSRAM。
- 不提交许可不允许分发的字库。

### Passport Keys 字库

应用嵌入四份只读 LVGL 字库：动态文字使用 16/20/28px、2bpp、无损 RLE 压缩，
固定界面使用未压缩 20px、4bpp。`passport-codepoints.txt` 列出 3,886 个动态字形，
`passport-ui-codepoints.txt` 列出 257 个固定字形。控件显式绑定字体；动态情景名称
与所有菜单行均使用 `passport_font_20_menu`。开始控制仍使用 16px 映射文字及固定
20px 实体键标签。压缩后的 16/28px 字库保留原来的像素和度量。
全部子集的位图偏移均在 20 位以内；`CONFIG_LV_FONT_FMT_TXT_LARGE=n`
将字形描述表的存储减半。
`CONFIG_LV_USE_FONT_COMPRESSED=y` 启用有界字形解码。

来源：[Noto Sans CJK SC Regular](https://github.com/notofonts/noto-cjk/blob/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf)，
SHA-256 为 `2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。
SIL Open Font License 保存在 [fonts/OFL.txt](fonts/OFL.txt)。子集更名为
`PassportKeysSubset`；[fonts/passport-keys-subset.otf](fonts/passport-keys-subset.otf)
保留实际轮廓。运行时无需读取字体文件。

清单包含 GB2312 一级的 3,755 个汉字、可打印 ASCII、常用中文标点、省略号、
方向箭头及全部固定界面字符。可复现的选择规则位于 `tools/keypad_font_inventory.py`。
共享 C／JavaScript 覆盖表拒绝新输入的不支持字符，包括生僻字及 emoji。
更改清单后，已有 v2／v3／v4／v5 配置的名称及快捷键仍会保留：UTF-8 原文保持完整，设备
以可见占位符显示缺字，浏览器显示原名供修改。通过浏览器保存前，需要替换名称中
不支持的字符。这份清单不代表全部中文或全部 Unicode。

使用 FontTools 4.61.1、lv_font_conv 1.5.3 和 Node.js 生成：

```sh
python3 tools/generate_keypad_fonts.py \
  --source /path/to/NotoSansCJKsc-Regular.otf \
  --converter /path/to/lv_font_conv/lv_font_conv.js --node /path/to/node
python3 tests/test_keypad_fonts.py
```

生成器检查源 cmap，收集 `main/main.c` 和 `main/keyboard/*.c` 的固定文字（排除
仅网页代码），更新清单和 C／网页覆盖数据，再以无字距调整方式转换，三种动态字号
使用压缩。修改设备文字后重新生成。静态测试解析四份 cmap，包括稀疏／密集映射中的缺字；
LVGL 预览验证实际字形查找、缺字负例与布局。Flash 容量和运行时堆／栈单独检查，
仍须实屏验收。参见[应用指南](../docs/assets/passport-keys.zh_CN.md)。

## 图片（images）

可复用的源图与生成的显示资产放在 `images/`。

| 文件                                                                   | 尺寸与格式           | 用途与来源                                                                                                                                     |
| ---------------------------------------------------------------------- | -------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------- |
| [`images/home.jpg`](images/home.jpg)                                   | 3840 × 2160，JPEG    | 嵌入中英文项目 README 的产品主图，突出 AI Passport 产品形象与开放、人人可创作的理念。                                                          |
| [`images/readme-hardware-specs.png`](images/readme-hardware-specs.png) | 2172 × 724，PNG RGBA | 保留为可选技术参考图，不再用于首页主视觉。于 2026-09-17 使用内置图像生成工具为本仓库生成；已根据文档中的硬件能力契约核对图中的六项标签与参数。 |
| [`images/logo-wordmark.png`](images/logo-wordmark.png)                 | 1648 × 336，PNG RGBA | 从仓库原始 `images/logo.png` 中精确裁切并去除背景的黑色字标；用于中英文项目 README 的浅色主题。                                                |
| [`images/logo-wordmark-dark.png`](images/logo-wordmark-dark.png)       | 1648 × 336，PNG RGBA | 提取字标的白色版本；README 使用 `<picture>` 在 GitHub 深色主题下显示。                                                                         |

- 使用描述性命名，并记录尺寸、像素格式、转换步骤与目标路径。
- 优先采用适合 240 × 320 RGB565 显示的格式，并纳入 Flash 与内部 RAM 考量。
- 许可允许时保留可编辑源文件，并记录来源与许可。
- 图片中不得包含设备二维码秘密、凭证或个人数据。

## 音乐与音效（music）

可复用的音乐与音效源码放在 `music/`。

- 记录来源、许可、采样率、位深、声道、转换命令与目标路径。
- 与当前 BSP 音频路径匹配时优先采用 16 kHz、16 位单声道 PCM。
- 嵌入音频前评估 Flash 与内部 RAM 成本；长录音应流式或分块。
- 无再分发许可不提交媒体文件。
