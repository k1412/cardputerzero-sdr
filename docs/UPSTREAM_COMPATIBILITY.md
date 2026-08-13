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
| [CardputerZero/Store](https://github.com/CardputerZero/Store) | `22d68d73fa4a2c174dc28c05df144d78a7e87e84` | Device-side dependency checks and local `.deb` installation through Debian package tools |
| [CardputerZero/AppBuilder](https://github.com/CardputerZero/AppBuilder) | `cfdaaf0860d5d33a55ffdf54b8473a6bf64ef785` | Store manifest, non-root validation, package ownership and publish flow |
| [CardputerZero/packages](https://github.com/CardputerZero/packages) | `f271a66ed8bb9a3d26a3d9baabf75c09b4d505c9` | Authoritative Store install-path, archive-safety, ownership and version policy |
| [CardputerZero/pi-gen](https://github.com/CardputerZero/pi-gen) | `ee05259f50e887eb7e23d5408edfd9dad5006462` | Raspberry Pi OS/Debian target image and normal-user group membership |
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
- The P0 runner cannot use APPLaunch's in-process `ExecBlocking` handoff from an
  SSH shell. For a full evidence run it therefore pauses and restores the
  official `APPLaunch.service` user unit around the child process, and refuses
  a concurrent Zero SDR instance. Read-only preflight never changes the unit.
- The package runs as the normal APPLaunch user and installs no root service,
  application-owned system udev rule, or maintainer script. The official BSP's
  glibc identifies its target as Debian 13, where the ARM64 `librtlsdr0` package
  owns `60-librtlsdr0.rules`; Zero SDR declares that dependency and leaves
  system USB policy to the distribution. The official image adds its normal
  user to `plugdev`, and the current Store backend resolves an unsatisfied local
  `.deb` dependency with `apt-get install`. Normal-user access remains a
  physical P0 gate instead of relying on privilege escalation.
- The official image verifier accepts `cardputerzero-overlay`,
  `cardputerzero-v3-overlay`, and `cardputerzero-v5-overlay` in the boot
  configuration. P0 records the base device-tree model and requires one of
  those overlay identities so evidence cannot come from a generic ARM64 host.
- Device-package CI downloads the Store install-path policy from the exact
  audited `CardputerZero/packages` revision, verifies the policy script digest,
  and applies it to the built `.deb`. It also mirrors the server's setuid,
  device-node, and maintainer-script safety checks.
- The current registry generator preserves `permissions` only when it is an
  object, although the AppBuilder schema document still describes a string
  array. Zero SDR uses the registry-consumed object form and CI checks the full
  offline keyboard/audio/external-hardware declaration so its Store entry does
  not silently lose permission metadata.

## Release audit procedure

Before promoting a hardware-tested release, compare upstream changes after the
revisions above. Review display/input initialization, process supervision,
`.desktop` validation, Store install/archive policy, ownership rules,
device-tree keymaps, audio routing, and the BSP release digest. Any relevant
behavioral change must be represented by CI, a host regression test, or an
explicit physical-device P0 check before Store submission.
