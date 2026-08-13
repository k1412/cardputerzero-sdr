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
- 自动增益与设备上报的手动增益档位、静音、深色/浅色主题
- 自动保存上次使用的频率、步进、增益、静音、主题和语言
- 运行时加载 RTL-SDR、带信道滤波的 WFM 音频与自动重连
- 每 30 秒输出结构化接收健康日志，记录 IQ、音频、DSP、重连和界面循环指标
- 随包提供无需 root 的 P0 预检与限时证据采集工具，并对硬件/资源信息做隐私筛选
- 经审计的 ARM64 Debian 安装包，内含固定版本的私有 librtlsdr 运行库
- 通过 Debian `librtlsdr0` 提供系统维护的 RTL-SDR `plugdev` 权限；应用不自带 udev 规则或 root 服务
- 十种界面语言：英语、简体中文、繁体中文、西班牙语、日语、韩语、法语、德语、巴西葡萄牙语和俄语
- 继承官方 CardputerZero 模板的 Linux/macOS/Windows 桌面模拟器支持
- 覆盖调谐、FFT/WFM DSP、RTL-SDR/ALSA 动态边界、重连、设置持久化和翻译完整性的主机测试

尚未完成或验证：

- 在实体 Cardputer Zero 上进行 RTL-SDR USB 采集和音频验证
- 在实体设备上安装 ARM64 `.deb`
- 提交 CardputerZero 应用商店

可变设置保存在 `$XDG_CONFIG_HOME/cardputerzero-sdr/` 或 `~/.config/cardputerzero-sdr/`；安装包中的 `/etc/cardputerzero-sdr.conf` 和仓库默认文件只作为只读初始值。

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

底栏始终显示主要操作提示。Z/C 连续调谐和设置页 F/X 移动支持受控长按；实体 TCA8418 键盘的设备树没有启用内核自动重复，因此应用会在用户态采用与 APPLaunch 一致的 500 毫秒起始延迟和 50 毫秒重复周期。步进、切换、确认和直接输入每次按下只执行一次。项目刻意避免破坏性操作和隐藏组合键。

在设置页的增益行中，左/右键会循环 `自动` 和当前调谐器实际报告的增益档位。目标 FC0012 提供 −9.9、−4.0、7.1、17.9 和 19.2 dB；其他 RTL-SDR 调谐器使用各自上报的列表。`G` 仍是一键切换自动/手动增益的快捷键。

## 桌面构建

依赖：CMake 3.31+、Ninja、Python 3.9+、支持 C++17 的编译器、SDL2、FreeType、libpng、libjpeg 和 zlib。找不到兼容的系统 `fmt` 时会获取固定版本 12.1.0。

Debian/Ubuntu：

```sh
sudo apt install build-essential cmake ninja-build python3 libsdl2-dev \
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

使用 `scripts/smoke_locales.sh` 可执行 50 项截图检查：十种语言各覆盖无线电、设置、直接调谐，以及无线电/设置页的音频故障状态。如只需截取直接调谐浮层，请从无线电页面启动并设置 `ZERO_SDR_DIRECT_ENTRY=103.9`。

使用 `scripts/capture_demo_states.sh` 可生成六种可重复的接收状态：离线演示、模拟设备 `LIVE`、设备上报的手动增益、`NO AUDIO`、直接调谐和 `NO DEVICE`。

## 运行诊断

应用默认每 30 秒向标准输出写入一条结构化 `diagnostics` 记录，其中包含累计连接/重试/读取错误、IQ 数据块与字节数、音频生成/写入/丢帧、ALSA 恢复与故障、DSP 处理耗时、界面循环次数和最大界面循环间隔。需要调整周期时，可将 `ZERO_SDR_DIAGNOSTICS_INTERVAL_MS` 设置为 100–3,600,000 毫秒。

ARM64 安装包会提供 `cardputerzero-sdr-p0`，这是一个无需 root 的设备预检与证据采集工具。运行时不要同时从 APPLaunch 启动第二个 Zero SDR 实例。先通过 SSH 或本地终端，以正常 APPLaunch 用户检查原生 320×170 帧缓冲、官方 APPLaunch/Cardputer 键盘路径、RTL-SDR USB 节点和可写 ALSA 播放节点：

```sh
cardputerzero-sdr-p0 --preflight-only
```

应用和预检工具都会依次采用 `LV_LINUX_FBDEV_DEVICE`、
`APPLAUNCH_LINUX_FBDEV_DEVICE`，最后才回退到 `/dev/fb0`；这与官方 Launcher
向外部应用交接显示设备的方式一致，也能正确覆盖重定向帧缓冲的测试环境。

然后启动有时间边界的 30 分钟测试；运行期间可正常使用实体按键操作应用：

```sh
cardputerzero-sdr-p0 --duration 1800
```

工具会在 `$XDG_STATE_HOME/cardputerzero-sdr/evidence/`（或 `~/.local/state/`）下创建权限收紧的证据目录，保存经过隐私筛选的硬件/权限快照、`app.log`、进程 CPU/内存/温度采样和退出结果。它会拒绝 root 运行，不采集主机名、网络状态或 USB 序列号，并通过已经测试的 `SIGTERM` 正常清理路径停止应用。把证据目录复制到开发机上的项目仓库，再汇总应用日志：

```sh
python3 scripts/summarize_diagnostics.py --p0 path/to/evidence/app.log
```

`--p0` 检查要求：连续运行至少 30 分钟、仅有一次无中断连接、RF/音频错误和丢帧均为零、IQ 吞吐在 4,096,000 字节/秒的 ±10% 内、未静音音频写入在 32,000 帧/秒的 ±10% 内，并且平均处理耗时小于每个 IQ 数据块 4,000 微秒的实时预算。最大单次处理耗时和界面循环间隔会被记录，用于建立实体设备基线，不会在没有实测数据时臆定发布阈值。完整步骤见[硬件测试计划](docs/DEVICE_TEST_PLAN.md)。

## Cardputer Zero 构建

交叉编译预设沿用官方 CardputerZero CMake 模板，并在需要时将固定版本 BSP 下载到 `.cache/`。主机必须安装 AArch64 交叉编译器。

```sh
cmake --workflow --preset cp0-cross-package
```

预期安装包：`dist/cardputerzero-sdr_0.1.0-2_arm64.deb`。

交叉编译成功不等同于实机验证。发布前必须完成[硬件测试计划](docs/DEVICE_TEST_PLAN.md)中的检查。

## 项目结构

```text
src/app/       生命周期、资源、模拟器和页面切换
src/audio/     有界队列与运行时加载的 ALSA 输出
src/device/    RTL-SDR 发现/采集与接收工作线程生命周期
src/dsp/       可复现演示、FFT 频谱、信道滤波和 WFM 解调
src/i18n/      语言目录与字体选择
src/model/     有边界的无线电状态
src/platform/  帧缓冲/DRM 选择和键盘/evdev 适配
src/view/      LVGL 页面、组件和 320×170 主题
src/viewmodel/ 状态格式化与界面操作
tests/         可在主机运行的单元及集成测试
docs/          架构、交互、语言和设备验证指南
```

更多信息请参阅[架构](docs/ARCHITECTURE.md)、[交互与按键](docs/UX.md)、[国际化](docs/I18N.md)、[上游兼容性审计](docs/UPSTREAM_COMPATIBILITY.md)和[贡献指南](CONTRIBUTING.md)。

## 应用商店发布策略

`app-builder.json` 已按 CardputerZero 工具准备，引用的截图均为原生 320×170 PNG。P0 实机检查通过前不会发布。应用不安装 root 系统服务、应用自有的系统 udev 规则或任何维护者脚本，也不需要网络或云端服务。安装包依赖 Debian 的 `librtlsdr0`，由该系统包维护的规则将受支持的 RTL-SDR 设备授权给 `plugdev`，当前设备商店会通过 Debian 包管理器解析这项依赖。正常 APPLaunch 用户的实机访问权限仍是 P0 发布门禁。设备打包 CI 还会下载官方商店仓库中固定提交的安装路径策略，在校验脚本摘要后对每个 `.deb` 执行同一套检查。

安装包名为 `cardputerzero-sdr`。应用商店中已有的 `zerosdr` 是另一个项目；本仓库不声明与其兼容或拥有该名称。

## 许可证

应用源代码采用 MIT 许可证。由于当前官方 BSP 不包含 librtlsdr，设备安装包会以独立 GPL-2.0-or-later 共享库的形式携带 librtlsdr 2.0.3，并在安装包中附带完整许可证。字体和图标字体资源的许可说明位于 `assets/fonts/`。生成的 Zero SDR 应用图标以本项目 MIT 许可证贡献。
