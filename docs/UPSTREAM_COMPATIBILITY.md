# Upstream compatibility audit

Zero SDR is an independent application, but its device boundary follows the
official Cardputer Zero platform. This file records the exact upstream state
used for release engineering; it is an audit snapshot, not a vendored fork.

## Audited snapshot

Last audited: 2026-08-14

| Upstream | Audited revision | Zero SDR dependency |
| --- | --- | --- |
| [CardputerZero/Template](https://github.com/CardputerZero/Template) | `d28fb262685a8a035ee354e905f737b504e1e62b` | CMake/LVGL device structure, asset and Debian layout |
| [CardputerZero/launcher](https://github.com/CardputerZero/launcher) | `83be7a3d6ccf646a6df50a7d4a914b7b0a738c4e` | APPLaunch process lifecycle, display/input environment, 320×170 framebuffer ownership |
| [CardputerZero/AppBuilder](https://github.com/CardputerZero/AppBuilder) | `cfdaaf0860d5d33a55ffdf54b8473a6bf64ef785` | Store manifest, non-root validation, package ownership and publish flow |
| [m5stack/m5stack-linux-dtoverlays](https://github.com/m5stack/m5stack-linux-dtoverlays) | `11cbbe808b03147617aa6e65799d2a9b6ff02cf8` | TCA8418 keymap/repeat behavior, audio and device permissions |
| [Cardputer Zero BSP v0.0.4](https://github.com/CardputerZero/launcher/releases/tag/v0.0.4) | `sdk_bsp.tar.gz`, SHA-256 `e51b6eb803ed08f450e459efbfe62dd0341440846f3be9d01da861fe6cfdebb0` | AArch64 sysroot used by the release package |

The BSP digest above is enforced both when downloading and when a cached
archive is reused by the cross toolchain. A custom BSP URL therefore requires
an explicitly matching `CM0_SDK_SHA256` value.

## Compatibility decisions

- The embedded UI uses the native 320×170 Linux framebuffer. Device selection
  follows `LV_LINUX_FBDEV_DEVICE`, then the
  `APPLAUNCH_LINUX_FBDEV_DEVICE` inherited by external applications, then the
  compiled `/dev/fb0` default. The P0 tool resolves and verifies the same path.
- Keyboard selection follows an explicit build setting, the Launcher LVGL and
  APPLaunch environment variables, the stable Cardputer Zero by-path node, and
  finally one evdev node only if it exposes every Zero SDR control group.
- The current TCA8418 overlay does not enable `keypad,autorepeat`. Zero SDR
  therefore synthesizes the Launcher's 500 ms initial delay and 50 ms repeat
  cadence, but defers to kernel `EV_REP` if a future overlay enables it.
- APPLaunch gives an external application exclusive framebuffer ownership and
  resumes after it exits. Zero SDR handles Esc normally and handles `SIGINT` and
  `SIGTERM` through the same clean receiver/audio shutdown path.
- The package runs as the normal APPLaunch user, installs no root service, and
  grants only narrow RTL2832U udev access. This matches current `czdev publish`
  validation rather than relying on privilege escalation.

## Release audit procedure

Before promoting a hardware-tested release, compare upstream changes after the
revisions above. Review display/input initialization, process supervision,
`.desktop` validation, package policy, device-tree keymaps, audio routing, and
the BSP release digest. Any relevant behavioral change must be represented by a
host regression test or an explicit physical-device P0 check before Store
submission.
