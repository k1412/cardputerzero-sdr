# Cardputer Zero 上的 Zero SDR

[English](README.md) | [简体中文](README.zh-CN.md)

[![CI](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/ci.yml)
[![设备安装包](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml/badge.svg)](https://github.com/k1412/cardputerzero-sdr/actions/workflows/device-package.yml)
[![许可证：MIT](https://img.shields.io/badge/license-MIT-35dcc8.svg)](LICENSE)

Zero SDR 是一款为 Cardputer Zero 打造的开源、键盘优先 RTL-SDR 接收机。界面以设备原生 320×170 分辨率渲染，并围绕实体 `F/X/Z/C`、Enter 和 Esc 按键设计。

![无线电频谱与瀑布图](screenshots/radio-dark.png)

> [!IMPORTANT]
> 本仓库仍处于发布前的硬件适配阶段。界面、可复现的离线演示、十语言目录和桌面测试均可工作；实时 RTL-SDR 采集、WFM 音频、USB 供电表现和性能尚未在实体 Cardputer Zero 上验证，因此目前不宣称已经在应用商店发布。

## 当前状态

- 原生 320×170 频谱和瀑布图界面
- 基于实测 FM 电台位置构造的可复现离线演示
- 适配 Cardputer 的按键映射与长按连续调谐
- 使用数字行直接输入 MHz，支持小数点、退格、Enter 和 Esc
- 与实测 FC0012 调谐器一致的 22.0–948.6 MHz 频率边界
- 从 10 kHz 到 1 MHz 的六档调谐步进
- 自动/手动增益、静音、深色/浅色主题
- 自动保存上次使用的频率、步进、增益、静音、主题和语言
- 运行时加载 RTL-SDR、带信道滤波的 WFM 音频与自动重连
- 经审计的 ARM64 Debian 安装包，内含固定版本的私有 librtlsdr 运行库
- 十种界面语言：英语、简体中文、繁体中文、西班牙语、日语、韩语、法语、德语、巴西葡萄牙语和俄语
- 继承官方 CardputerZero 模板的 Linux/macOS/Windows 桌面模拟器支持
- 覆盖调谐、FFT/WFM DSP、RTL-SDR/ALSA 动态边界、重连、设置持久化和翻译完整性的主机测试

尚未完成或验证：

- 在实体 Cardputer Zero 上进行 RTL-SDR USB 采集和音频验证
- 在实体设备上安装 ARM64 `.deb`
- 提交 CardputerZero 应用商店

## 按键

| 按键 | 无线电页面 | 设置页面 |
| --- | --- | --- |
| `F` / 上 | 增大调谐步进 | 上一项 |
| `X` / 下 | 减小调谐步进 | 下一项 |
| `Z` / 左 | 向下调谐 | 上一个值 |
| `C` / 右 | 向上调谐 | 下一个值 |
| Enter | 打开设置 | 返回无线电页面 |
| Esc | 退出到启动器 | 返回无线电页面 |
| `G` | 切换自动/手动增益 | 切换增益模式 |
| `M` | 静音/取消静音 | 静音/取消静音 |
| `L` | 切换下一种语言 | 切换下一种语言 |
| `T` | 切换深色/浅色主题 | 切换深色/浅色主题 |
| `0`–`9`、`.` | 打开 MHz 直接输入 | — |
| 退格 | 删除直接输入的字符 | — |

底栏始终显示主要操作提示。调谐键支持长按连续触发；项目刻意避免破坏性操作和隐藏组合键。

## 桌面构建

依赖：CMake 3.31+、Ninja、支持 C++17 的编译器、SDL2、FreeType、libpng、libjpeg 和 zlib。找不到兼容的系统 `fmt` 时会获取固定版本 12.1.0。

Debian/Ubuntu：

```sh
sudo apt install build-essential cmake ninja-build libsdl2-dev \
  libfreetype-dev libpng-dev libjpeg-dev zlib1g-dev
cmake --preset linux-x86-64
cmake --build --preset linux-x86-64-dbg
ctest --test-dir build/linux-x86-64 -C Debug --output-on-failure
./build/linux-x86-64/Debug/cardputerzero-sdr
```

无界面截图冒烟测试：

```sh
SDL_VIDEODRIVER=dummy \
ZERO_SDR_LOCALE=zh-CN \
ZERO_SDR_START_PAGE=settings \
ZERO_SDR_SCREENSHOT=/tmp/zero-sdr.png \
ZERO_SDR_SCREENSHOT_EXIT=1 \
./build/linux-x86-64/Debug/cardputerzero-sdr
```

使用 `scripts/smoke_locales.sh` 可检查十种语言的无线电、设置和直接调谐三种状态。如只需截取直接调谐浮层，请从无线电页面启动并设置 `ZERO_SDR_DIRECT_ENTRY=103.9`。

## Cardputer Zero 构建

交叉编译预设沿用官方 CardputerZero CMake 模板，并在需要时将固定版本 BSP 下载到 `.cache/`。主机必须安装 AArch64 交叉编译器。

```sh
cmake --workflow --preset cp0-cross-package
```

预期安装包：`dist/cardputerzero-sdr_0.1.0-1_arm64.deb`。

交叉编译成功不等同于实机验证。发布前必须完成[硬件测试计划](docs/DEVICE_TEST_PLAN.md)中的检查。

## 项目结构

```text
src/app/       生命周期、资源、模拟器和页面切换
src/audio/     有界队列与运行时加载的 ALSA 输出
src/device/    RTL-SDR 发现/采集与接收工作线程生命周期
src/dsp/       可复现演示、FFT 频谱、信道滤波和 WFM 解调
src/i18n/      语言目录与字体选择
src/model/     有边界的无线电状态
src/platform/  SDL/DRM 显示和键盘/evdev 适配
src/view/      LVGL 页面、组件和 320×170 主题
src/viewmodel/ 状态格式化与界面操作
tests/         可在主机运行的单元及集成测试
docs/          架构、交互、语言和设备验证指南
```

更多信息请参阅[架构](docs/ARCHITECTURE.md)、[交互与按键](docs/UX.md)、[国际化](docs/I18N.md)和[贡献指南](CONTRIBUTING.md)。

## 应用商店发布策略

`app-builder.json` 已按 CardputerZero 工具准备，引用的截图均为原生 320×170 PNG。P0 实机检查通过前不会发布。应用不会安装 root 系统服务，也不需要网络或云端服务。

安装包名为 `cardputerzero-sdr`。应用商店中已有的 `zerosdr` 是另一个项目；本仓库不声明与其兼容或拥有该名称。

## 许可证

应用源代码采用 MIT 许可证。由于当前官方 BSP 不包含 librtlsdr，设备安装包会以独立 GPL-2.0-or-later 共享库的形式携带 librtlsdr 2.0.3，并在安装包中附带完整许可证。字体和图标字体资源的许可说明位于 `assets/fonts/`。生成的 Zero SDR 应用图标以本项目 MIT 许可证贡献。
