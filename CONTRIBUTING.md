# Contributing

Issues and pull requests are welcome. Before changing behavior, describe the Cardputer Zero use case and the physical-key flow; desktop-only interactions must not become required device interactions.

## Development checks

```sh
cmake --preset linux-x86-64
cmake --build --preset linux-x86-64-dbg
ctest --test-dir build/linux-x86-64 -C Debug --output-on-failure
```

For UI changes, attach a native 320×170 screenshot and verify at least English, Simplified Chinese, Japanese, Korean, and Russian. For radio/DSP changes, add deterministic vectors or recorded-data fixtures that can run without hardware.

Keep hardware access out of LVGL callbacks, keep queues bounded, and document measurements instead of guessing device capacity. Do not commit dongle serials, USB traces containing unrelated devices, credentials, build outputs, or fetched dependency trees.

Commits should be focused and include an SPDX license identifier in new source files. By contributing, you agree that your contribution is licensed under the repository's MIT license unless a file clearly states another compatible license.
